// re1_archive.cpp - Implementacao do arquivador R1PA.
#include "re1_archive.h"
#include "zeebo_brew.h"
#include "../platform.h"

#include <stdlib.h>
#include <string.h>

// Layout do arquivo (little-endian):
//   +0  'R','1','P','A'
//   +4  u16 version = 1
//   +6  u16 flags = 0
//   +8  u32 entryCount
//   +12 u32 indexOffset
//   +16 u32 indexSize   (== entryCount * sizeof(RE1ArcEntry))
//   +20 u32 dataOffset
//   +24 u32 reserved
//   +28 u32 reserved
//   ... entries ...
#define RE1PA_HEADER_SIZE 32u
#define RE1PA_MAX_ENTRIES 20000u

static IFile*        s_pak   = NULL;
static RE1ArcEntry*  s_index = NULL;
static uint32_t      s_count = 0;

static uint16_t Rd16LE(const uint8_t* p)
{
    return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}

static uint32_t Rd32LE(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int ReadPakAt(uint32_t offset, void* dst, uint32_t len)
{
    if (!s_pak || !s_pak->pvt) return 0;
    if (s_pak->pvt->Seek(s_pak, _SEEK_START, (int32_t)offset) != AEE_SUCCESS) {
        return 0;
    }
    uint32_t done = 0;
    while (done < len) {
        int32_t n = s_pak->pvt->Read(s_pak, (uint8_t*)dst + done, len - done);
        if (n <= 0) break;
        done += (uint32_t)n;
    }
    return (done == len) ? 1 : 0;
}

// Normaliza para o mesmo formato do indice: sem './', barras '/', MAIUSCULAS,
// sem prefixos 'assets/' e 'usa/'.
static void NormalizeName(const char* path, char* out, size_t cap)
{
    const char* p = path ? path : "";
    while (p[0] == '.' && (p[1] == '/' || p[1] == '\\')) p += 2;

    size_t j = 0;
    for (; *p && j + 1 < cap; ++p) {
        char c = *p;
        if (c == '\\') c = '/';
        if (c == '/') {
            while (p[1] == '/' || p[1] == '\\') ++p;
            if (j == 0) continue;
        }
        if (c >= 'a' && c <= 'z') c = (char)(c - ('a' - 'A'));
        out[j++] = c;
    }
    out[j] = '\0';

    for (int pass = 0; pass < 2; ++pass) {
        size_t n = strlen(out);
        if (n > 7 && memcmp(out, "ASSETS/", 7) == 0) {
            memmove(out, out + 7, n - 7 + 1);
        } else if (n > 4 && memcmp(out, "USA/", 4) == 0) {
            memmove(out, out + 4, n - 4 + 1);
        } else {
            break;
        }
    }
}

static int NameICmp(const char* a, const char* b)
{
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;
        if (ca >= 'a' && ca <= 'z') ca = (unsigned char)(ca - 32);
        if (cb >= 'a' && cb <= 'z') cb = (unsigned char)(cb - 32);
        if (ca != cb) return (int)ca - (int)cb;
        ++a;
        ++b;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int RE1Arc_Open(const char* pakPath)
{
    if (!pakPath || !g_Zeebo.pFileMgr || !g_Zeebo.pFileMgr->pvt) return 0;

    char norm[260];
    plat_normalize_path(pakPath, norm, sizeof(norm));
    IFile* f = g_Zeebo.pFileMgr->pvt->OpenFile(g_Zeebo.pFileMgr, norm, _OFM_READ);
    if (!f) {
        Zeebo_Log("[R1PA] '%s' nao encontrado\n", norm);
        return 0;
    }

    AEEFileInfo info;
    memset(&info, 0, sizeof(info));
    if (f->pvt->GetInfo(f, &info) != AEE_SUCCESS || info.dwSize < RE1PA_HEADER_SIZE + 88u) {
        f->pvt->Release(f);
        return 0;
    }

    s_pak = f;
    uint8_t hdr[RE1PA_HEADER_SIZE];
    if (!ReadPakAt(0, hdr, sizeof(hdr)) || memcmp(hdr, "R1PA", 4) != 0 ||
        Rd16LE(hdr + 4) != 1) {
        RE1Arc_Close();
        return 0;
    }

    uint32_t count = Rd32LE(hdr + 8);
    uint32_t indexOff = Rd32LE(hdr + 12);
    uint32_t indexSize = Rd32LE(hdr + 16);
    if (count == 0 || count > RE1PA_MAX_ENTRIES ||
        indexSize != count * (uint32_t)sizeof(RE1ArcEntry) ||
        indexOff + indexSize > info.dwSize) {
        RE1Arc_Close();
        return 0;
    }

    RE1ArcEntry* index = (RE1ArcEntry*)malloc(indexSize);
    if (!index || !ReadPakAt(indexOff, index, indexSize)) {
        free(index);
        RE1Arc_Close();
        return 0;
    }

    s_index = index;
    s_count = count;
    Zeebo_Log("[R1PA] '%s': %u entradas, %u bytes de indice\n",
              norm, count, indexSize);
    return 1;
}

void RE1Arc_Close(void)
{
    if (s_pak && s_pak->pvt) s_pak->pvt->Release(s_pak);
    s_pak = NULL;
    free(s_index);
    s_index = NULL;
    s_count = 0;
}

int RE1Arc_IsOpen(void)  { return (s_pak && s_index) ? 1 : 0; }
uint32_t RE1Arc_EntryCount(void) { return s_count; }

const RE1ArcEntry* RE1Arc_Find(const char* path)
{
    if (!RE1Arc_IsOpen()) return NULL;
    char wanted[RE1PA_NAME_MAX];
    NormalizeName(path, wanted, sizeof(wanted));

    // O indice e' gravado ordenado pelo gerador; busca binaria.
    uint32_t lo = 0, hi = s_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        int cmp = NameICmp(s_index[mid].name, wanted);
        if (cmp < 0) lo = mid + 1u;
        else hi = mid;
    }
    if (lo < s_count && NameICmp(s_index[lo].name, wanted) == 0) {
        return &s_index[lo];
    }
    return NULL;
}

int RE1Arc_OpenFile(const char* path, RE1ArcFile* out)
{
    if (!out) return 0;
    const RE1ArcEntry* e = RE1Arc_Find(path);
    if (!e) return 0;
    out->entry = e;
    out->pos = 0;
    return 1;
}

int RE1Arc_ReadAt(const RE1ArcEntry* e, uint32_t off, void* dst, uint32_t len)
{
    if (!e || !RE1Arc_IsOpen()) return 0;
    if (off > e->size || len > e->size - off) return 0;
    if (len == 0) return 1;
    return ReadPakAt(e->offset + off, dst, len);
}

int RE1Arc_Read(RE1ArcFile* f, void* dst, uint32_t len)
{
    if (!f || !f->entry) return 0;
    if (!RE1Arc_ReadAt(f->entry, f->pos, dst, len)) return 0;
    f->pos += len;
    return 1;
}

uint32_t RE1Arc_Size(const RE1ArcFile* f)
{
    return (f && f->entry) ? f->entry->size : 0u;
}

int RE1Arc_Seek(RE1ArcFile* f, int32_t pos, int whence)
{
    if (!f || !f->entry) return 0;
    int32_t base = 0;
    if (whence == 1) base = (int32_t)f->pos;
    else if (whence == 2) base = (int32_t)f->entry->size;
    int64_t np = (int64_t)base + pos;
    if (np < 0) np = 0;
    if (np > f->entry->size) np = f->entry->size;
    f->pos = (uint32_t)np;
    return 1;
}
