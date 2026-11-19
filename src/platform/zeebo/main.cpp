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
#include "splash_image.h"

#include <stdlib.h>
#include <string.h>

extern void CleanupVideoConfigAndSaveAllSettings(void);

static BOOL        s_displayApplyPending = FALSE;
static BOOL        s_splashActive        = FALSE;
static MarniHandle s_splashTex           = MARNI_NULL_HANDLE;
static DWORD       s_splashStartMs       = 0;

static void FreeStartupSplash(void)
{
    if (s_splashTex != MARNI_NULL_HANDLE) {
        MarniDX* dx = Marni_DX();
        if (dx) {
            dx->DestroyTexture(s_splashTex);
        }
        s_splashTex = MARNI_NULL_HANDLE;
    }
    s_splashActive = FALSE;
}

static void InitStartupSplash(void)
{
    MarniDX* dx = Marni_DX();
    if (!dx || !dx->IsReady()) return;

    size_t totalPixels = (size_t)ZEEBO_SPLASH_W * (size_t)ZEEBO_SPLASH_H;
    uint32_t* rgba = (uint32_t*)malloc(totalPixels * sizeof(uint32_t));
    if (!rgba) return;

    size_t out = 0;
    for (uint32_t r = 0; r < ZEEBO_SPLASH_RLE_RUNS && out < totalPixels; ++r) {
        uint32_t count = ZEEBO_SPLASH_RLE[r * 2 + 0];
        uint32_t px    = ZEEBO_SPLASH_RLE[r * 2 + 1];
        if (out + count > totalPixels) count = (uint32_t)(totalPixels - out);
        for (uint32_t k = 0; k < count; ++k) {
            rgba[out++] = px;
        }
    }

    s_splashTex = dx->CreateTexture(ZEEBO_SPLASH_W, ZEEBO_SPLASH_H, 32, rgba, NULL, NULL);
    free(rgba);

    if (s_splashTex != MARNI_NULL_HANDLE) {
        s_splashActive  = TRUE;
        s_splashStartMs = plat_time_ms();
    }
}

static BOOL TickStartupSplash(void)
{
    if (!s_splashActive) return FALSE;

    DWORD elapsed = plat_time_ms() - s_splashStartMs;
    if (elapsed >= 3200u) {
        FreeStartupSplash();
        return FALSE;
    }

    MarniDX* dx = Marni_DX();
    if (!dx || s_splashTex == MARNI_NULL_HANDLE) {
        FreeStartupSplash();
        return FALSE;
    }

    uint32_t alpha = 255u;
    if (elapsed < 350u) {
        alpha = (elapsed * 255u) / 350u;
    } else if (elapsed > 2750u) {
        DWORD rem = 3200u - elapsed;
        alpha = (rem * 255u) / 450u;
    }
    if (alpha > 255u) alpha = 255u;

    DWORD bw = 640, bh = 480;
    dx->GetBackBufferSize(&bw, &bh);
    dx->Clear(0.0f, 0.0f, 0.0f, 1.0f);
    dx->DrawSprite(0.0f, 0.0f, (float)bw, (float)bh,
                   0.0f, 0.0f, 1.0f, 1.0f,
                   (alpha << 24) | 0x00FFFFFFu,
                   s_splashTex, MARNI_SAMPLER_LINEAR, MARNI_BLEND_ALPHA);
    dx->Present();
    return TRUE;
}

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

    InitStartupSplash();

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

    if (TickStartupSplash()) {
        return;
    }

    if (plat_video_is_active()) {
        plat_video_tick();
    } else {
        main_loop();
    }
}

extern "C" void Zeebo_OnAppStop(void)
{
    FreeStartupSplash();
    CleanupVideoConfigAndSaveAllSettings();
    MarniDX_DestroyGlobal();
}
