// video.cpp - Zeebo FMV playback backend (AVI + Cinepak -> GLES1 texture).
#include "../platform.h"
#include "zeebo_brew.h"
#include "avi_demux.h"
#include "cinepak.h"

#include "../../marni/MarniDX.h"

#include <stdlib.h>

namespace {

PlatVideoOverlayCallback s_overlayCb = NULL;

AviDemux*       s_avi       = NULL;
CinepakDecoder* s_cinepak   = NULL;
MarniHandle     s_tex       = MARNI_NULL_HANDLE;

int    s_width       = 0;
int    s_height      = 0;
double s_fps         = 10.0;
int    s_frameIndex  = -1;
int    s_baseFrame   = 0;
int    s_playToFrame = 0;
DWORD  s_startMs     = 0;
BOOL   s_active      = FALSE;
BOOL   s_endEvent    = FALSE;
BOOL   s_eof         = FALSE;

void FreeVideoResources(void)
{
    if (s_tex != MARNI_NULL_HANDLE) {
        MarniDX* dx = Marni_DX();
        if (dx) {
            dx->DestroyTexture(s_tex);
        }
        s_tex = MARNI_NULL_HANDLE;
    }
    Cinepak_Destroy(s_cinepak);
    s_cinepak = NULL;
    AviDemux_Close(s_avi);
    s_avi = NULL;

    s_width = s_height = 0;
    s_frameIndex  = -1;
    s_baseFrame   = 0;
    s_playToFrame = 0;
    s_active      = FALSE;
    s_eof         = FALSE;
}

BOOL DecodeOneFrame(void)
{
    if (!s_avi || !s_cinepak) return FALSE;
    const uint8_t* pkt = NULL;
    uint32_t pktSize = 0;
    if (!AviDemux_NextVideoFrame(s_avi, &pkt, &pktSize)) {
        s_eof = TRUE;
        return FALSE;
    }
    Cinepak_DecodeFrame(s_cinepak, pkt, pktSize);
    ++s_frameIndex;
    return TRUE;
}

void SeekToFrame(int targetFrame)
{
    if (!s_avi || !s_cinepak) return;
    if (targetFrame < s_frameIndex) {
        AviDemux_Rewind(s_avi);
        s_frameIndex = -1;
        s_eof = FALSE;
    }
    while (s_frameIndex < targetFrame) {
        if (!DecodeOneFrame()) break;
    }
    s_baseFrame = (s_frameIndex >= 0) ? s_frameIndex : 0;
    s_startMs   = plat_time_ms();
}

void PresentFrame(void)
{
    MarniDX* dx = Marni_DX();
    const uint8_t* rgba = Cinepak_GetRGBA(s_cinepak);
    if (!dx || !rgba || s_width <= 0 || s_height <= 0) return;

    if (s_tex == MARNI_NULL_HANDLE) {
        s_tex = dx->CreateTexture(s_width, s_height, 32, rgba, NULL, NULL);
    } else {
        dx->ReplaceTexture(s_tex, s_width, s_height, rgba);
    }

    dx->Clear(0.0f, 0.0f, 0.0f, 1.0f);

    int frameMs = (s_frameIndex >= 0 && s_fps > 0.0)
        ? (int)((double)s_frameIndex / s_fps * 1000.0) : 0;

    BOOL handled = FALSE;
    if (s_overlayCb && s_tex != MARNI_NULL_HANDLE) {
        handled = s_overlayCb((DWORD32)s_tex, frameMs);
    }

    if (!handled && s_tex != MARNI_NULL_HANDLE) {
        DWORD bw = 640, bh = 480;
        dx->GetBackBufferSize(&bw, &bh);
        dx->DrawSprite(0.0f, 0.0f, (float)bw, (float)bh,
                       0.0f, 0.0f, 1.0f, 1.0f,
                       0xFFFFFFFFu, s_tex,
                       MARNI_SAMPLER_LINEAR, MARNI_BLEND_DISABLE);
    }

    dx->Present();
}

} // namespace

void plat_video_set_overlay_callback(PlatVideoOverlayCallback callback)
{
    s_overlayCb = callback;
}

BOOL plat_video_init(void)
{
    return TRUE;
}

BOOL plat_video_open_and_play(const char* path, int playToMs)
{
    Zeebo_Log("[VID] open_and_play '%s' playToMs=%d\n", path ? path : "(null)", playToMs);
    FreeVideoResources();
    s_endEvent = FALSE;

    s_avi = AviDemux_Open(path);
    if (!s_avi) {
        s_endEvent = TRUE;
        return FALSE;
    }

    const AviVideoInfo* info = AviDemux_GetInfo(s_avi);
    s_width  = info->width;
    s_height = info->height;
    s_fps    = (info->fps > 0.0) ? info->fps : 10.0;

    s_cinepak = Cinepak_Create(s_width, s_height);
    if (!s_cinepak) {
        FreeVideoResources();
        s_endEvent = TRUE;
        return FALSE;
    }

    s_playToFrame = (playToMs > 0)
        ? (int)((double)playToMs / 1000.0 * s_fps + 0.5) : 0;

    s_active = TRUE;
    SeekToFrame(0);
    PresentFrame();
    return TRUE;
}

int plat_video_probe_duration_ms(const char* path)
{
    AviDemux* avi = AviDemux_Open(path);
    if (!avi) return 0;
    int ms = AviDemux_GetDurationMs(avi);
    AviDemux_Close(avi);
    return ms;
}

void plat_video_play_from(int fromMs)
{
    if (!s_avi) return;
    int fromFrame = (int)((double)fromMs / 1000.0 * s_fps + 0.5);
    s_playToFrame = 0;
    s_endEvent    = FALSE;
    s_active      = TRUE;
    SeekToFrame(fromFrame);
    PresentFrame();
}

void plat_video_stop(void)
{
    s_active = FALSE;
}

void plat_video_close(void)
{
    FreeVideoResources();
    s_endEvent = FALSE;
}

BOOL plat_video_is_active(void)
{
    return s_active;
}

BOOL plat_video_take_end_event(void)
{
    BOOL had = s_endEvent;
    s_endEvent = FALSE;
    return had;
}

void plat_video_tick(void)
{
    if (!s_active || !s_avi) return;

    double elapsedSec = (double)(plat_time_ms() - s_startMs) / 1000.0;
    int targetFrame   = s_baseFrame + (int)(elapsedSec * s_fps);

    while (s_frameIndex < targetFrame) {
        if (!DecodeOneFrame()) break;
        if (s_playToFrame > 0 && s_frameIndex >= s_playToFrame) {
            s_active   = FALSE;
            s_endEvent = TRUE;
            return;
        }
    }

    if (s_eof) {
        s_active   = FALSE;
        s_endEvent = TRUE;
        return;
    }

    PresentFrame();
}

void plat_video_set_window_mode(BOOL forVideo)
{
    (void)forVideo;
}
