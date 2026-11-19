// crash.cpp - Crash, breadcrumb, and file/dbgprintf logger for Zeebo.
#include "../platform.h"
#include "zeebo_brew.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static IFile* s_logFile = NULL;

extern "C" void Zeebo_Log(const char* fmt, ...)
{
    if (!fmt) return;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    buf[n] = '\0';

    if (g_Zeebo.pHelpers && g_Zeebo.pHelpers->pfnDbgPrintf) {
        g_Zeebo.pHelpers->pfnDbgPrintf("%s", buf);
    }

    if (g_Zeebo.pFileMgr && g_Zeebo.pFileMgr->pvt) {
        if (!s_logFile) {
            s_logFile = g_Zeebo.pFileMgr->pvt->OpenFile(
                g_Zeebo.pFileMgr, "re1_zeebo.log",
                (OpenFileMode)(_OFM_READWRITE | _OFM_CREATE));
        }
        if (s_logFile && s_logFile->pvt) {
            s_logFile->pvt->Write(s_logFile, buf, (uint32_t)n);
        }
    }
}

void crashlog_install(void)
{
    Zeebo_Log("[RE1-Zeebo] crashlog_install (FileMgr=%p EGL=%p GL=%p HID=%p)\n",
              g_Zeebo.pFileMgr, g_Zeebo.pEGL, g_Zeebo.pGL, g_Zeebo.pHID);
}

void crashlog_mark(const char* step)
{
    if (step) {
        Zeebo_Log("[RE1-Zeebo] mark: %s\n", step);
    }
}
