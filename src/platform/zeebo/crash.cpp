// crash.cpp - Crash, breadcrumb, and file/dbgprintf logger for Zeebo.
#include "../platform.h"
#include "zeebo_brew.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef AEE_SIMULATOR
static IFile* s_logFile = NULL;
#endif

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

#ifdef AEE_SIMULATOR
    FILE* simf = fopen("C:\\Users\\Esther\\AppData\\Local\\Temp\\re1_sim.log", "a");
    if (simf) {
        fputs(buf, simf);
        if (buf[n - 1] != '\n') fputc('\n', simf);
        fclose(simf);
    }
#endif

    if (g_Zeebo.pHelpers && g_Zeebo.pHelpers->pfnDbgPrintf) {
        g_Zeebo.pHelpers->pfnDbgPrintf("%s", buf);
    }

#ifdef AEE_SIMULATOR
    // Log em arquivo SOMENTE no simulador (depuracao). No hardware fica
    // desligado: toda escrita IFileMgr vai para a flash interna do app, e um
    // log crescente deixaria "vestigios". O pfnDbgPrintf acima nao cria
    // arquivo algum.
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
#else
    (void)buf;
#endif
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
