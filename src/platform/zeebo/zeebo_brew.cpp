// zeebo_brew.cpp - Qualcomm BREW 4.0.2 module & applet entry glue.
//
// Implements AEEMod_Load, IModule, and IApplet so zeebx-emu (and real Zeebo
// hardware) can load the compiled .mod binary, create the applet instance,
// and dispatch EVT_APP_START / EVT_APP_STOP / EVT_KEY / timer ticks.
#include "zeebo_brew.h"
#include "../platform.h"

#include <string.h>
#ifdef AEE_SIMULATOR
#include <windows.h>
#include <stdio.h>
static void SimSetModuleCwd(void)
{
    HMODULE hm = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)&SimSetModuleCwd, &hm) && hm) {
        char path[MAX_PATH];
        DWORD len = GetModuleFileNameA(hm, path, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            char* slash = strrchr(path, '\\');
            if (slash) {
                *slash = '\0';
                SetCurrentDirectoryA(path);
            }
        }
    }
}
static void SimTrace(const char* msg)
{
    FILE* f = fopen("C:\\Users\\Esther\\AppData\\Local\\Temp\\re1_sim.log", "a");
    if (f) {
        fputs(msg, f);
        fputc('\n', f);
        fclose(f);
    }
}
#else
#define SimTrace(msg) ((void)0)
#endif

ZeeboRuntime g_Zeebo = { 0 };

// ---------------------------------------------------------------------------
// IApplet implementation
// ---------------------------------------------------------------------------
typedef struct Re1Applet {
    const IAppletVtbl* pvt;
    uint32_t           refCount;
} Re1Applet;

static uint32_t Applet_AddRef(IApplet* po)
{
    Re1Applet* self = (Re1Applet*)po;
    return ++self->refCount;
}

static uint32_t Applet_Release(IApplet* po)
{
    Re1Applet* self = (Re1Applet*)po;
    if (self->refCount > 0) {
        --self->refCount;
    }
    if (self->refCount == 0 && g_Zeebo.pHelpers && g_Zeebo.pHelpers->pfnFree) {
        g_Zeebo.pHelpers->pfnFree(self);
        g_Zeebo.pApplet = NULL;
        return 0;
    }
    return self->refCount;
}

static void FrameTimerCallback(void* pUser)
{
    (void)pUser;
    if (!g_Zeebo.bRunning) {
        return;
    }
    uint32_t startMs = plat_time_ms();
    Zeebo_OnFrameTick();
    uint32_t elapsed = plat_time_ms() - startMs;

    // Target 33 ms (~30 fps) total frame period matching RE1's original 0x21 ms tick,
    // while always yielding at least 8 ms so the BREW/emulator event loop stays responsive.
    if (g_Zeebo.bRunning && g_Zeebo.pShell) {
        int32_t delay = (elapsed >= 25u) ? 8 : (int32_t)(33u - elapsed);
        ISHELL_SetTimer(g_Zeebo.pShell, delay, FrameTimerCallback, NULL);
    }
}

static boolean Applet_HandleEvent(IApplet* po, AEEEvent eCode, uint16_t wParam, uint32_t dwParam)
{
    (void)po;
    switch (eCode) {
    case EVT_APP_START:
        SimTrace("EVT_APP_START enter");
        if (!Zeebo_OnAppStart()) {
            SimTrace("Zeebo_OnAppStart failed");
            return FALSE;
        }
        SimTrace("EVT_APP_START ok");
        g_Zeebo.bRunning = TRUE;
        if (g_Zeebo.pShell) {
            ISHELL_SetTimer(g_Zeebo.pShell, 1, FrameTimerCallback, NULL);
        }
        return TRUE;

    case EVT_APP_STOP:
        g_Zeebo.bRunning = FALSE;
        if (g_Zeebo.pShell) {
            ISHELL_CancelTimer(g_Zeebo.pShell, FrameTimerCallback, NULL);
        }
        Zeebo_OnAppStop();
        return TRUE;

    case EVT_APP_SUSPEND:
        if (g_Zeebo.pfnAudioSuspend) {
            g_Zeebo.pfnAudioSuspend();
        }
        if (g_Zeebo.pShell) {
            ISHELL_CancelTimer(g_Zeebo.pShell, FrameTimerCallback, NULL);
        }
        return TRUE;

    case EVT_APP_RESUME:
        if (g_Zeebo.bRunning && g_Zeebo.pShell) {
            ISHELL_SetTimer(g_Zeebo.pShell, 33, FrameTimerCallback, NULL);
        }
        return TRUE;

    case EVT_APP_NO_SLEEP:
        return TRUE;

    default:
        return FALSE;
    }
}

static const IAppletVtbl s_AppletVtbl = {
    Applet_AddRef,
    Applet_Release,
    Applet_HandleEvent
};

// ---------------------------------------------------------------------------
// IModule implementation
// ---------------------------------------------------------------------------
typedef struct Re1Module {
    const IModuleVtbl* pvt;
    uint32_t           refCount;
} Re1Module;

static uint32_t Module_AddRef(IModule* po)
{
    Re1Module* self = (Re1Module*)po;
    return ++self->refCount;
}

static uint32_t Module_Release(IModule* po)
{
    Re1Module* self = (Re1Module*)po;
    if (self->refCount > 0) {
        --self->refCount;
    }
    if (self->refCount == 0 && g_Zeebo.pHelpers && g_Zeebo.pHelpers->pfnFree) {
        g_Zeebo.pHelpers->pfnFree(self);
        return 0;
    }
    return self->refCount;
}

static int Module_CreateInstance(IModule* po, IShell* pIShell, AEECLSID ClsId, void** ppObj)
{
    SimTrace("Module_CreateInstance enter");
    (void)po;
    (void)ClsId;
    if (!ppObj || !pIShell || !g_Zeebo.pHelpers || !g_Zeebo.pHelpers->pfnMalloc) {
        SimTrace("Module_CreateInstance bad args");
        return EFAILED;
    }

    g_Zeebo.pShell = pIShell;

    // Acquire core BREW services up front; optional ones (HID, Sound) may be NULL.
    SimTrace("CreateInstance FILEMGR");
    ISHELL_CreateInstance(pIShell, AEECLSID_FILEMGR, (void**)&g_Zeebo.pFileMgr);
    SimTrace("CreateInstance DISPLAY");
    ISHELL_CreateInstance(pIShell, AEECLSID_DISPLAY, (void**)&g_Zeebo.pDisplay);
    SimTrace("CreateInstance EGL");
    ISHELL_CreateInstance(pIShell, AEECLSID_EGL,     (void**)&g_Zeebo.pEGL);
    SimTrace("CreateInstance GL");
    ISHELL_CreateInstance(pIShell, AEECLSID_GL,      (void**)&g_Zeebo.pGL);
    if (!g_Zeebo.pGL) {
        SimTrace("CreateInstance GLES11");
        ISHELL_CreateInstance(pIShell, AEECLSID_GLES11, (void**)&g_Zeebo.pGL);
    }
    SimTrace("CreateInstance HID");
    ISHELL_CreateInstance(pIShell, AEECLSID_HID,       (void**)&g_Zeebo.pHID);
    SimTrace("CreateInstance SOUND");
    ISHELL_CreateInstance(pIShell, AEECLSID_SOUND,     (void**)&g_Zeebo.pSound);
    SimTrace("CreateInstance MEDIAUTIL");
    ISHELL_CreateInstance(pIShell, AEECLSID_MEDIAUTIL, (void**)&g_Zeebo.pMediaUtil);

    SimTrace("Alloc Re1Applet");
    Re1Applet* app = (Re1Applet*)g_Zeebo.pHelpers->pfnMalloc(sizeof(Re1Applet));
    if (!app) {
        return ENOMEMORY;
    }
    app->pvt = &s_AppletVtbl;
    app->refCount = 1;

    g_Zeebo.pApplet = (IApplet*)app;
    *ppObj = app;
    SimTrace("Module_CreateInstance ok");
    return AEE_SUCCESS;
}

static void Module_FreeResources(IModule* po, IFileMgr* pfm, void* pRes)
{
    (void)po;
    (void)pfm;
    (void)pRes;
}

static const IModuleVtbl s_ModuleVtbl = {
    Module_AddRef,
    Module_Release,
    Module_CreateInstance,
    Module_FreeResources
};

// ---------------------------------------------------------------------------
// AEEMod_Load - Primary module entry point (placed at .text.entry / 0x0)
// ---------------------------------------------------------------------------
#ifdef AEE_SIMULATOR
static void RunCppGlobalConstructors(void)
{
    // Win32 DLL CRT runs C++ global constructors automatically during DllMain.
}
#else
typedef void (*InitFn)(void);
extern "C" InitFn __init_array_start[];
extern "C" InitFn __init_array_end[];

static void RunCppGlobalConstructors(void)
{
    static boolean s_initDone = FALSE;
    if (s_initDone) return;
    s_initDone = TRUE;
    for (InitFn* fn = __init_array_start; fn < __init_array_end; ++fn) {
        if (*fn) (*fn)();
    }
}
#endif

static AEEHelperFuncs* ResolveHelpers(void* ph)
{
    if (ph) {
        return (AEEHelperFuncs*)ph;
    }
#ifdef AEE_SIMULATOR
    return NULL;
#else
    uintptr_t addr = ((uintptr_t)&AEEMod_Load) & ~(uintptr_t)3u;
    for (int i = 0; i < 256; ++i) {
        const uint32_t* p = (const uint32_t*)(addr - (uintptr_t)(i * 4));
        if (p[2] == 0x57455242u /* "BREW" */) {
            return *(AEEHelperFuncs* const*)(p - 1);
        }
    }
    return *(((AEEHelperFuncs* const*)(uintptr_t)&AEEMod_Load) - 1);
#endif
}

#ifdef AEE_SIMULATOR
extern "C" __declspec(dllexport)
#else
extern "C" __attribute__((section(".text.entry")))
#endif
int AEEMod_Load(IShell* ps, void* ph, IModule** ppMod)
{
#ifdef AEE_SIMULATOR
    SimSetModuleCwd();
#endif
    SimTrace("AEEMod_Load enter");
    if (!ps || !ppMod) {
        return EBADPARM;
    }

    AEEHelperFuncs* helpers = ResolveHelpers(ph);
    if (!helpers || !helpers->pfnMalloc) {
        SimTrace("AEEMod_Load helpers NULL");
        return EFAILED;
    }

    g_Zeebo.pShell   = ps;
    g_Zeebo.pHelpers = helpers;

    RunCppGlobalConstructors();

    SimTrace("AEEMod_Load malloc Re1Module");
    Re1Module* mod = (Re1Module*)helpers->pfnMalloc(sizeof(Re1Module));
    if (!mod) {
        return ENOMEMORY;
    }
    mod->pvt = &s_ModuleVtbl;
    mod->refCount = 1;

    *ppMod = (IModule*)mod;
    SimTrace("AEEMod_Load ok");
    return AEE_SUCCESS;
}
