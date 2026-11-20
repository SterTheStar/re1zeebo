// platform.cpp - BREW 4.0.2 implementation of src/platform/platform.h.
//
// Provides filesystem I/O (via IFileMgr / IFile), monotonic time, stack
// allocation for TaskScheduler, and debug logging for Zeebo.
#include "../platform.h"
#include "zeebo_brew.h"
#include "../../Globals.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------
DWORD plat_time_ms(void)
{
    if (g_Zeebo.pHelpers && g_Zeebo.pHelpers->pfnGetUpTimeMS) {
        return g_Zeebo.pHelpers->pfnGetUpTimeMS();
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Filesystem (IFileMgr / IFile)
// ---------------------------------------------------------------------------
const char* plat_normalize_path(const char* path, char* buffer, size_t size)
{
    if (!path || !buffer || size == 0) {
        return path;
    }
    // Strip leading "./" or ".\" so BREW IFileMgr resolves relative to /mod/<app>/
    while (path[0] == '.' && (path[1] == '/' || path[1] == '\\')) {
        path += 2;
    }
    size_t j = 0;
    for (size_t i = 0; path[i] != '\0' && j + 1 < size; ++i) {
        char c = (path[i] == '\\') ? '/' : path[i];
        if (c == '/' && j > 0 && buffer[j - 1] == '/') {
            continue;
        }
        buffer[j++] = c;
    }
    buffer[j] = '\0';
    return buffer;
}

BOOL plat_exe_dir(char* out, size_t size)
{
    if (!out || size < 2) {
        return FALSE;
    }
    // In BREW / zeebx-emu, all paths are rooted in the module's VFS directory.
    out[0] = '.';
    out[1] = '\0';
    return TRUE;
}

BOOL plat_path_is_absolute(const char* path)
{
    if (!path || path[0] == '\0') {
        return FALSE;
    }
    return (path[0] == '/') || (path[0] != '\0' && path[1] == ':');
}

BOOL plat_mkdir(const char* path)
{
    if (!path || !g_Zeebo.pFileMgr || !g_Zeebo.pFileMgr->pvt->MkDir) {
        return FALSE;
    }
    char norm[260];
    plat_normalize_path(path, norm, sizeof(norm));
    int rc = g_Zeebo.pFileMgr->pvt->MkDir(g_Zeebo.pFileMgr, norm);
    return (rc == AEE_SUCCESS || rc == EFILEEXISTS) ? TRUE : FALSE;
}

BOOL plat_dir_list(const char* path, PlatDirVisit visit, void* user)
{
    if (!path || !visit || !g_Zeebo.pFileMgr) {
        return FALSE;
    }
    char norm[260];
    plat_normalize_path(path, norm, sizeof(norm));

    // Enumerate directories first, then files.
    for (int pass = 0; pass < 2; ++pass) {
        boolean isDir = (pass == 0) ? TRUE : FALSE;
        if (g_Zeebo.pFileMgr->pvt->EnumInit(g_Zeebo.pFileMgr, norm, isDir) == AEE_SUCCESS) {
            AEEFileInfo info;
            while (g_Zeebo.pFileMgr->pvt->EnumNext(g_Zeebo.pFileMgr, &info)) {
                const char* base = strrchr(info.szName, '/');
                const char* name = base ? (base + 1) : info.szName;
                if (name[0] != '\0' && strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
                    visit(name, isDir ? TRUE : FALSE, user);
                }
            }
        }
    }
    return TRUE;
}

BOOL plat_file_write(const char* path, const void* data, size_t size)
{
    if (!path || !g_Zeebo.pFileMgr) {
        return FALSE;
    }
    char norm[260];
    plat_normalize_path(path, norm, sizeof(norm));

    IFile* f = g_Zeebo.pFileMgr->pvt->OpenFile(
        g_Zeebo.pFileMgr, norm, (OpenFileMode)(_OFM_READWRITE | _OFM_CREATE));
    if (!f) {
        return FALSE;
    }
    if (f->pvt->Truncate) {
        f->pvt->Truncate(f, 0);
    }

    uint32_t written = 0;
    if (data && size > 0) {
        written = f->pvt->Write(f, data, (uint32_t)size);
    }
    f->pvt->Release(f);
    return (written == (uint32_t)size) ? TRUE : FALSE;
}

size_t plat_readable_bytes(const void* p)
{
    // BREW/ARM flat memory map has no VirtualQuery; module code+data starts at 0x00010000.
    // Returning 4096 here previously clamped 256x256 8bpp TIMs to 16 rows (256x16)!
    return ((uintptr_t)p >= 0x00010000u) ? (size_t)0x04000000u : 0u;
}

void* plat_file_read_all(const char* path, size_t* outSize)
{
    if (outSize) {
        *outSize = 0;
    }
    if (!path || !g_Zeebo.pFileMgr) {
        return NULL;
    }

    char norm[260];
    plat_normalize_path(path, norm, sizeof(norm));

    IFile* f = g_Zeebo.pFileMgr->pvt->OpenFile(g_Zeebo.pFileMgr, norm, _OFM_READ);
    if (!f) {
        return NULL;
    }

    AEEFileInfo info;
    memset(&info, 0, sizeof(info));
    if (f->pvt->GetInfo(f, &info) != AEE_SUCCESS || info.dwSize == 0) {
        f->pvt->Release(f);
        return NULL;
    }

    size_t allocSize = ((size_t)info.dwSize & ~3u) + 0x10u;
    void* buf = malloc(allocSize);
    if (!buf) {
        f->pvt->Release(f);
        return NULL;
    }
    memset(buf, 0, allocSize);

    int32_t totalRead = 0;
    while ((uint32_t)totalRead < info.dwSize) {
        int32_t n = f->pvt->Read(f, (uint8_t*)buf + totalRead, info.dwSize - (uint32_t)totalRead);
        if (n <= 0) {
            break;
        }
        totalRead += n;
    }
    f->pvt->Release(f);

    if (outSize) {
        *outSize = (size_t)totalRead;
    }
    return buf;
}

unsigned int plat_file_mtime(const char* path)
{
    if (!path || !g_Zeebo.pFileMgr) {
        return 0;
    }
    char norm[260];
    plat_normalize_path(path, norm, sizeof(norm));

    AEEFileInfo info;
    if (g_Zeebo.pFileMgr->pvt->GetInfo(g_Zeebo.pFileMgr, norm, &info) != AEE_SUCCESS) {
        return 0;
    }
    return (unsigned int)info.dwCreationDate;
}

// ---------------------------------------------------------------------------
// Diagnostics & environment
// ---------------------------------------------------------------------------
void plat_debug_output(const char* s)
{
    if (s && g_Zeebo.pHelpers && g_Zeebo.pHelpers->pfnDbgPrintf) {
        g_Zeebo.pHelpers->pfnDbgPrintf("%s", s);
    }
}

BOOL plat_is_debugger_present(void)
{
    return TRUE;
}

DWORD plat_env_get(const char* name, char* buffer, DWORD size)
{
    (void)name;
    if (buffer && size > 0) {
        buffer[0] = '\0';
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Task stacks & fatal error
// ---------------------------------------------------------------------------
void* plat_alloc_guarded_stacks(int count, size_t stackSize, size_t guardSize)
{
    if (count <= 0 || stackSize == 0) {
        return NULL;
    }
    size_t total = guardSize + (size_t)count * (stackSize + guardSize);
    uint8_t* mem = (uint8_t*)malloc(total);
    if (!mem) {
        return NULL;
    }
    memset(mem, 0, total);
    return mem + guardSize;
}

void plat_fatal(const char* message)
{
    plat_debug_output(message ? message : "[FATAL] RE1 Zeebo fatal error\n");
    crashlog_mark(message ? message : "fatal");
    g_bQuitFlag = TRUE;
    if (g_Zeebo.pShell) {
        ISHELL_CloseApplet(g_Zeebo.pShell, TRUE);
    }
}

void plat_window_destroy(HWND window)
{
    (void)window;
}

void plat_cursor_show(BOOL show)
{
    (void)show;
}
