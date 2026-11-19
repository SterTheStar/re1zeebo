// main.cpp - Zeebo / BREW 4.0.2 applet lifecycle & 33 ms frame pump.
//
// Counterpart of src/platform/linux/main.cpp. Instead of owning a blocking
// main() loop, BREW drives frames via ISHELL_SetTimer callbacks routed from
// zeebo_brew.cpp into Zeebo_OnAppStart / Zeebo_OnFrameTick / Zeebo_OnAppStop.
#include "zeebo_brew.h"

#include "../../Globals.h"
#include "../platform.h"
#include "../../system/AssetPath.h"
#include "../../system/ConfigFile.h"
#include "../../marni/MarniDX.h"
#include "../../marni/MarniSystem.h"

#include <string.h>

extern void CleanupVideoConfigAndSaveAllSettings(void);

static BOOL s_displayApplyPending = FALSE;

void plat_display_request_apply(void)
{
    s_displayApplyPending = TRUE;
}

void plat_request_restart(void)
{
    g_bQuitFlag = TRUE;
}

void plat_request_quit(void)
{
    g_bQuitFlag = TRUE;
    if (g_Zeebo.pShell) {
        ISHELL_CloseApplet(g_Zeebo.pShell, TRUE);
    }
}

BOOL plat_display_desktop_size(int* width, int* height)
{
    // Zeebo standard composite TV output is 640x480 (NTSC / PAL-M).
    if (width)  *width = 640;
    if (height) *height = 480;
    return TRUE;
}

extern "C" boolean Zeebo_OnAppStart(void)
{
    crashlog_install();
    crashlog_mark("Zeebo_OnAppStart");

    g_dwScreenWidth  = 640;
    g_dwScreenHeight = 480;
    g_bFullScreen    = TRUE;
    g_bQuitFlag      = FALSE;

    ConfigFile_Load();

    StartSoundSystemAsync(NULL);
    TaskScheduler_Init();

    InitializeMarniSystem();
    if (!Marni_DX() || !Marni_DX()->IsReady()) {
        plat_fatal("Failed to initialize Marni system on Zeebo.");
        return FALSE;
    }

    g_bWindowFocused = TRUE;
    g_bWindowActive  = TRUE;
    g_bQuitFlag      = FALSE;
    g_dwGameTimer1   = plat_time_ms();

    return TRUE;
}

extern "C" void Zeebo_OnFrameTick(void)
{
    if (g_bQuitFlag) {
        plat_request_quit();
        return;
    }

    if (s_displayApplyPending) {
        s_displayApplyPending = FALSE;
        MarniDX* dx = Marni_DX();
        if (dx) {
            dx->ChangeDisplayMode(g_dwScreenWidth, g_dwScreenHeight, g_bFullScreen);
        }
    }

    g_dwGameTimer1 = plat_time_ms();

    if (plat_video_is_active()) {
        plat_video_tick();
    } else {
        main_loop();
    }
}

extern "C" void Zeebo_OnAppStop(void)
{
    CleanupVideoConfigAndSaveAllSettings();
    MarniDX_DestroyGlobal();
}
