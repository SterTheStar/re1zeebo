// main.cpp - Zeebo / BREW 4.0.2 applet lifecycle & 33 ms frame pump.
//
// BREW counterpart of the old PC main loops (those builds were removed).
// Instead of owning a blocking main() loop, BREW drives frames via
// ISHELL_SetTimer callbacks routed from
// zeebo_brew.cpp into Zeebo_OnAppStart / Zeebo_OnFrameTick / Zeebo_OnAppStop.
#include "zeebo_brew.h"

#include "../../Globals.h"
#include "../platform.h"
#include "../../system/AssetPath.h"
#include "../../system/ConfigFile.h"
#include "../../marni/MarniDX.h"
#include "../../marni/MarniSystem.h"
#include "pandora_logo.h"
#include "splash_image.h"
#include "re1_archive.h"

#include <stdlib.h>
#include <string.h>

extern void CleanupVideoConfigAndSaveAllSettings(void);

enum StartupStage {
    STARTUP_NONE = 0,
    STARTUP_PANDORA = 1,
    STARTUP_WARNING = 2,
};

static BOOL         s_displayApplyPending = FALSE;
static StartupStage s_startupStage        = STARTUP_NONE;
static MarniHandle  s_splashTex           = MARNI_NULL_HANDLE;
static DWORD        s_splashStartMs       = 0;

static MarniHandle CreateRleTexture(int w, int h, const uint32_t* rle, uint32_t runs)
{
    MarniDX* dx = Marni_DX();
    if (!dx || !dx->IsReady()) return MARNI_NULL_HANDLE;

    size_t totalPixels = (size_t)w * (size_t)h;
    uint32_t* rgba = (uint32_t*)malloc(totalPixels * sizeof(uint32_t));
    if (!rgba) return MARNI_NULL_HANDLE;

    size_t out = 0;
    for (uint32_t r = 0; r < runs && out < totalPixels; ++r) {
        uint32_t count = rle[r * 2 + 0];
        uint32_t px    = rle[r * 2 + 1];
        if (out + count > totalPixels) count = (uint32_t)(totalPixels - out);
        for (uint32_t k = 0; k < count; ++k) {
            rgba[out++] = px;
        }
    }

    MarniHandle tex = dx->CreateTexture(w, h, 32, rgba, NULL, NULL);
    free(rgba);
    return tex;
}

static void FreeStartupSplash(void)
{
    if (s_splashTex != MARNI_NULL_HANDLE) {
        MarniDX* dx = Marni_DX();
        if (dx) {
            dx->DestroyTexture(s_splashTex);
        }
        s_splashTex = MARNI_NULL_HANDLE;
    }
    s_startupStage = STARTUP_NONE;
}

static void StartWarningSplash(void)
{
    FreeStartupSplash();
    s_splashTex = CreateRleTexture(ZEEBO_SPLASH_W, ZEEBO_SPLASH_H,
                                   ZEEBO_SPLASH_RLE, ZEEBO_SPLASH_RLE_RUNS);
    if (s_splashTex != MARNI_NULL_HANDLE) {
        s_startupStage  = STARTUP_WARNING;
        s_splashStartMs = plat_time_ms();
    }
}

static void InitStartupSplash(void)
{
    s_splashTex = CreateRleTexture(ZEEBO_PANDORA_W, ZEEBO_PANDORA_H,
                                   ZEEBO_PANDORA_RLE, ZEEBO_PANDORA_RLE_RUNS);
    if (s_splashTex != MARNI_NULL_HANDLE) {
        s_startupStage  = STARTUP_PANDORA;
        s_splashStartMs = plat_time_ms();
    } else {
        StartWarningSplash();
    }
}

static BOOL TickStartupSplash(void)
{
    if (s_startupStage == STARTUP_NONE) return FALSE;

    DWORD totalMs   = (s_startupStage == STARTUP_PANDORA) ? 2800u : 4200u;
    DWORD fadeOutMs = 500u;
    DWORD elapsed   = plat_time_ms() - s_splashStartMs;

    if (elapsed >= totalMs) {
        if (s_startupStage == STARTUP_PANDORA) {
            StartWarningSplash();
            return (s_startupStage != STARTUP_NONE);
        }
        FreeStartupSplash();
        return FALSE;
    }

    MarniDX* dx = Marni_DX();
    if (!dx || s_splashTex == MARNI_NULL_HANDLE) {
        FreeStartupSplash();
        return FALSE;
    }

    uint32_t alpha = 255u;
    if (elapsed < 400u) {
        alpha = (elapsed * 255u) / 400u;
    } else if (elapsed > (totalMs - fadeOutMs)) {
        DWORD rem = totalMs - elapsed;
        alpha = (rem * 255u) / fadeOutMs;
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
    // Zeebo Developer Guide 11.3: udata/ e' a unica pasta onde o jogo pode
    // gravar dados gerados. Cria antes de qualquer log/config/save.
    plat_mkdir("udata");

    crashlog_install();
    crashlog_mark("Zeebo_OnAppStart");

    crashlog_mark("RE1Arc_Open");
    RE1Arc_Open("RE1DATA.PAK");   // se faltar, cai para arquivos soltos

    g_dwScreenWidth  = 640;
    g_dwScreenHeight = 480;
    g_bFullScreen    = TRUE;
    g_bQuitFlag      = FALSE;

    crashlog_mark("ConfigFile_Load");
    ConfigFile_Load();

    crashlog_mark("StartSoundSystemAsync");
    StartSoundSystemAsync(NULL);
    crashlog_mark("TaskScheduler_Init");
    TaskScheduler_Init();

    crashlog_mark("InitializeMarniSystem");
    InitializeMarniSystem();
    if (!Marni_DX() || !Marni_DX()->IsReady()) {
        plat_fatal("Failed to initialize Marni system on Zeebo.");
        return FALSE;
    }

    g_bWindowFocused = TRUE;
    g_bWindowActive  = TRUE;
    g_bQuitFlag      = FALSE;
    g_dwGameTimer1   = plat_time_ms();

    crashlog_mark("InitStartupSplash");
    InitStartupSplash();

    crashlog_mark("Zeebo_OnAppStart done");
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

    if (g_bMCINotifyEnabled) {
        if (g_bMCINotifyFlag) {
            UpdateVideoPlayback();
            return;
        }
        g_bMCINotifyFlag = TRUE;
    }

    main_loop();
}

extern "C" void Zeebo_OnAppStop(void)
{
    FreeStartupSplash();
    CleanupVideoConfigAndSaveAllSettings();
    RE1Arc_Close();
    MarniDX_DestroyGlobal();
}
