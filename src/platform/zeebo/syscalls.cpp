// syscalls.cpp - Newlib bare-metal / BREW 4.0.2 syscall & runtime glue for Zeebo.
//
// Routes C/C++ heap (_sbrk) and stdio file I/O (_open/_read/_write/_lseek/_close)
// to BREW's heap / IFileMgr so std::string, std::vector, fopen, etc. work on Zeebo.
#include "../platform.h"
#include "zeebo_brew.h"
#include "re1_lz4.h"
#include "re1_archive.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

extern "C" {

void* __dso_handle = 0;

// Keep .bss small so it fits inside zeebx-emu's MODULE_BSS_SLACK (1 MiB).
// Primary dynamic allocations go through BREW's pfnMalloc (HEAP_BASE 0x10000000).
static uint8_t  s_heap[64 * 1024];
static size_t   s_heapOffset = 0;

void* malloc(size_t size)
{
    if (g_Zeebo.pHelpers && g_Zeebo.pHelpers->pfnMalloc) {
        return g_Zeebo.pHelpers->pfnMalloc((uint32_t)(size ? size : 1));
    }
    return NULL;
}

void free(void* ptr)
{
    if (ptr && g_Zeebo.pHelpers && g_Zeebo.pHelpers->pfnFree) {
        g_Zeebo.pHelpers->pfnFree(ptr);
    }
}

void* realloc(void* ptr, size_t size)
{
    if (g_Zeebo.pHelpers && g_Zeebo.pHelpers->pfnRealloc) {
        return g_Zeebo.pHelpers->pfnRealloc(ptr, (uint32_t)(size ? size : 1));
    }
    return NULL;
}

void* calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void* p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void* _sbrk(ptrdiff_t incr)
{
    if (incr < 0) {
        size_t dec = (size_t)(-incr);
        if (dec > s_heapOffset) {
            errno = ENOMEM;
            return (void*)-1;
        }
        s_heapOffset -= dec;
        return s_heap + s_heapOffset;
    }

    size_t aligned = (s_heapOffset + 7u) & ~7u;
    if (aligned + (size_t)incr > sizeof(s_heap)) {
        errno = ENOMEM;
        return (void*)-1;
    }
    void* prev = s_heap + aligned;
    s_heapOffset = aligned + (size_t)incr;
    return prev;
}

#define MAX_FDS 32
#define RE1_MEMFILE_MAX (4u * 1024u * 1024u)
static IFile*   s_fdTable[MAX_FDS];
static uint8_t  s_fdUsed[MAX_FDS];
static uint8_t* s_fdMem[MAX_FDS];   // arquivo R1LZ descompactado em RAM
static uint32_t s_fdMemSize[MAX_FDS];
static uint32_t s_fdPos[MAX_FDS];
static uint32_t s_fdSize[MAX_FDS];

int _open(const char* name, int flags, ...)
{
    if (!name || !g_Zeebo.pFileMgr || !g_Zeebo.pFileMgr->pvt) {
        errno = ENOENT;
        return -1;
    }

    int acc = flags & 3;

    // Leitura: normalizacao simples. Escrita: filtro anti-escape — nada pode
    // sair de /mod/<app>/ na eNAND e parar na NAND de 128 MB do sistema.
    char norm[260];
    if (acc != 0) {
        if (!plat_write_path_safe(name, norm, sizeof(norm))) {
            errno = EACCES;
            return -1;
        }
    } else {
        plat_normalize_path(name, norm, sizeof(norm));
    }

    OpenFileMode mode = _OFM_READ;
    if (acc == 1 || acc == 2) {
        mode = _OFM_READWRITE;
        if (flags & 0x0200 /* O_CREAT */) {
            mode = (OpenFileMode)(_OFM_READWRITE | _OFM_CREATE);
        }
    }

    // Leitura de asset dentro do PAK: serve entirely da RAM (o _open normal
    // adiante continua cuidando de saves/config/logs, que ficam soltos).
    if (acc == 0 && RE1Arc_IsOpen()) {
        RE1ArcFile vf;
        if (RE1Arc_OpenFile(name, &vf)) {
            uint32_t packedSize = RE1Arc_Size(&vf);
            uint8_t* raw = (uint8_t*)malloc(packedSize ? packedSize : 1u);
            if (!raw || !RE1Arc_Read(&vf, raw, packedSize)) {
                free(raw);
                errno = EIO;
                return -1;
            }
            void* out = raw;
            size_t outLen = packedSize;
            int rc = RE1LZ_Unpack(raw, packedSize, &out, &outLen);
            if (rc < 0) {
                free(raw);
                errno = EIO;
                return -1;
            }
            if (rc == 1) free(raw);
            for (int fd = 3; fd < MAX_FDS; ++fd) {
                if (!s_fdUsed[fd]) {
                    s_fdUsed[fd]   = 1;
                    s_fdTable[fd]  = NULL;
                    s_fdMem[fd]    = (uint8_t*)out;
                    s_fdMemSize[fd]= (uint32_t)outLen;
                    s_fdPos[fd]    = 0;
                    s_fdSize[fd]   = (uint32_t)outLen;
                    return fd;
                }
            }
            free(out);
            errno = EMFILE;
            return -1;
        }
    }

    IFile* f = g_Zeebo.pFileMgr->pvt->OpenFile(g_Zeebo.pFileMgr, norm, mode);
    static int s_openLogCount = 0;
    if (s_openLogCount < 120) {
        ++s_openLogCount;
        Zeebo_Log("[OPEN #%d] '%s' mode=%d -> %p\n", s_openLogCount, norm, (int)mode, f);
    }
    if (!f) {
        errno = ENOENT;
        return -1;
    }

    if ((acc == 1 || acc == 2) && (flags & 0x0400 /* O_TRUNC */) && f->pvt->Truncate) {
        f->pvt->Truncate(f, 0);
    }

    AEEFileInfo info;
    memset(&info, 0, sizeof(info));
    uint32_t fileSize = 0;
    int infoRc = f->pvt->GetInfo(f, &info);
    if (infoRc == AEE_SUCCESS) {
        fileSize = info.dwSize;
    }
    if ((acc == 1 || acc == 2) && (flags & 0x0400 /* O_TRUNC */)) {
        fileSize = 0;
    }
    if (s_openLogCount <= 120) {
        Zeebo_Log("[INFO] '%s' rc=%d size=%u\n", norm, infoRc, (unsigned)fileSize);
    }

    // Arquivos pequenos embrulhados em R1LZ sao inflados uma vez no _open e
    // servidos da RAM. Arquivos grandes (FMV) ficam em streaming via IFile.
    uint8_t* mem = NULL;
    uint32_t memSize = 0;
    if (fileSize >= RE1LZ_HEADER_SIZE && fileSize <= RE1_MEMFILE_MAX) {
        uint8_t* raw = (uint8_t*)malloc(fileSize);
        if (raw) {
            uint32_t got = 0;
            if (f->pvt->Seek(f, _SEEK_START, 0) == AEE_SUCCESS) {
                while (got < fileSize) {
                    int32_t n = f->pvt->Read(f, raw + got, fileSize - got);
                    if (n <= 0) break;
                    got += (uint32_t)n;
                }
            }
            if (got == fileSize) {
                void* out = NULL;
                size_t outLen = 0;
                int rc = RE1LZ_Unpack(raw, fileSize, &out, &outLen);
                if (rc == 1) {
                    mem = (uint8_t*)out;
                    memSize = (uint32_t)outLen;
                    fileSize = memSize;
                } else if (rc < 0) {
                    Zeebo_Log("[R1LZ] corrompido (stdio): %s\n", norm);
                    free(raw);
                    f->pvt->Release(f);
                    errno = EIO;
                    return -1;
                }
            }
            free(raw);
        }
    }

    for (int fd = 3; fd < MAX_FDS; ++fd) {
        if (!s_fdUsed[fd]) {
            s_fdUsed[fd]     = 1;
            s_fdTable[fd]    = f;
            s_fdMem[fd]      = mem;
            s_fdMemSize[fd]  = memSize;
            s_fdPos[fd]      = 0;
            s_fdSize[fd]     = fileSize;
            return fd;
        }
    }
    free(mem);
    f->pvt->Release(f);
    errno = EMFILE;
    return -1;
}

int _close(int fd)
{
    if (fd >= 3 && fd < MAX_FDS && s_fdUsed[fd]) {
        free(s_fdMem[fd]);
        s_fdMem[fd]      = NULL;
        s_fdMemSize[fd]  = 0;
        if (s_fdTable[fd]) {
            s_fdTable[fd]->pvt->Release(s_fdTable[fd]);
            s_fdTable[fd] = NULL;
        }
        s_fdUsed[fd]  = 0;
        s_fdPos[fd]   = 0;
        s_fdSize[fd]  = 0;
        return 0;
    }
    errno = EBADF;
    return -1;
}

int _read(int fd, void* ptr, int len)
{
    if (fd >= 3 && fd < MAX_FDS && s_fdUsed[fd] && ptr && len >= 0) {
        if (s_fdMem[fd]) {
            uint32_t avail = (s_fdPos[fd] < s_fdMemSize[fd])
                           ? s_fdMemSize[fd] - s_fdPos[fd] : 0u;
            uint32_t n = ((uint32_t)len < avail) ? (uint32_t)len : avail;
            memcpy(ptr, s_fdMem[fd] + s_fdPos[fd], n);
            s_fdPos[fd] += n;
            return (int)n;
        }
        int32_t n = s_fdTable[fd]->pvt->Read(s_fdTable[fd], ptr, (uint32_t)len);
        if (n > 0) {
            s_fdPos[fd] += (uint32_t)n;
            return (int)n;
        }
        return 0;
    }
    errno = EBADF;
    return -1;
}

int _write(int fd, const void* ptr, int len)
{
    if (fd == 1 || fd == 2) {
        if (ptr && len > 0 && g_Zeebo.pHelpers && g_Zeebo.pHelpers->pfnDbgPrintf) {
            char buf[256];
            int n = (len < 255) ? len : 255;
            memcpy(buf, ptr, (size_t)n);
            buf[n] = '\0';
            g_Zeebo.pHelpers->pfnDbgPrintf("%s", buf);
        }
        return len;
    }
    if (fd >= 3 && fd < MAX_FDS && s_fdUsed[fd] && s_fdTable[fd] && ptr && len >= 0) {
        uint32_t n = s_fdTable[fd]->pvt->Write(s_fdTable[fd], ptr, (uint32_t)len);
        s_fdPos[fd] += n;
        if (s_fdPos[fd] > s_fdSize[fd]) s_fdSize[fd] = s_fdPos[fd];
        return (int)n;
    }
    errno = EBADF;
    return -1;
}

int _lseek(int fd, int ptr, int dir)
{
    if (fd >= 3 && fd < MAX_FDS && s_fdUsed[fd]) {
        int32_t newPos = 0;
        FileSeekType st = _SEEK_START;
        if (dir == 0) {
            st = _SEEK_START;
            newPos = ptr;
        } else if (dir == 1) {
            st = _SEEK_CURRENT;
            newPos = (int32_t)s_fdPos[fd] + ptr;
        } else if (dir == 2) {
            st = _SEEK_END;
            newPos = (int32_t)s_fdSize[fd] + ptr;
        }
        if (newPos < 0) newPos = 0;
        if (!s_fdMem[fd]) {
            s_fdTable[fd]->pvt->Seek(s_fdTable[fd], st, (int32_t)ptr);
        }
        s_fdPos[fd] = (uint32_t)newPos;
        return (int)s_fdPos[fd];
    }
    errno = EBADF;
    return -1;
}

int _fstat(int fd, struct stat* st)
{
    if (!st) return -1;
    memset(st, 0, sizeof(*st));
    if (fd >= 3 && fd < MAX_FDS && s_fdUsed[fd] && s_fdMem[fd]) {
        st->st_mode = S_IFREG;
        st->st_size = (off_t)s_fdSize[fd];
        return 0;
    }
    if (fd >= 3 && fd < MAX_FDS && s_fdUsed[fd] && s_fdTable[fd]) {
        AEEFileInfo info;
        if (s_fdTable[fd]->pvt->GetInfo(s_fdTable[fd], &info) == AEE_SUCCESS) {
            st->st_mode = S_IFREG;
            st->st_size = (off_t)info.dwSize;
            return 0;
        }
    }
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int fd)
{
    return (fd >= 0 && fd <= 2) ? 1 : 0;
}

int _kill(int pid, int sig)
{
    (void)pid; (void)sig;
    errno = EINVAL;
    return -1;
}

int _getpid(void)
{
    return 1;
}

int _gettimeofday(struct timeval* tv, void* tz)
{
    (void)tz;
    if (tv) {
        uint32_t ms = (g_Zeebo.pHelpers && g_Zeebo.pHelpers->pfnGetUpTimeMS)
            ? g_Zeebo.pHelpers->pfnGetUpTimeMS() : 0u;
        tv->tv_sec  = (long)(ms / 1000u);
        tv->tv_usec = (long)((ms % 1000u) * 1000u);
    }
    return 0;
}

void _fini(void) {}

void _exit(int status)
{
    (void)status;
    if (g_Zeebo.pShell) {
        ISHELL_CloseApplet(g_Zeebo.pShell, TRUE);
    }
    while (1) {}
}

} // extern "C"
