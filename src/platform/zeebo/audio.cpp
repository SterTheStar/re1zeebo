// audio.cpp - Zeebo / BREW 4.0.2 IMedia audio backend & DirectSound bank manager.
//
// Faithful to Qualcomm BREW 4.0.2 SDK (AEEIMedia.h / AEEMediaUtil.h):
//   - Registers IMedia_RegisterNotify callback for async state tracking
//   - Checks IMedia_GetState(pbStateChanging) before Stop / Release / status queries
//   - Uses AEE_MAX_VOLUME (100), MM_PARM_PAN (0..MM_MAX_PAN), MM_PARM_PLAY_REPEAT
//   - Stops active playback on EVT_APP_SUSPEND
#include "../platform.h"
#include "zeebo_brew.h"
#include "re1_archive.h"
#include "pcm_source.h"

#include "../../Globals.h"
#include "../../marni/MarniSound.h"
#include "../../system/AssetPath.h"
#include "../../system/AudioFile.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BANKS 81

enum BrewPlayState {
    BPS_IDLE = 0,
    BPS_STARTING,
    BPS_PLAYING,
    BPS_PAUSED,
    BPS_DONE,
    BPS_ABORTED
};

struct ZeeboSoundBank {
    BOOL          active;
    BOOL          loop;
    int           vol;        // DirectSound millibels (0 = full, -10000 = silent)
    int           pan;        // DirectSound pan (-10000 left, 0 center, +10000 right)
    int           slot;
    DWORD         cmdMs;
    DWORD         startMs;
    DWORD         durationMs;
    BrewPlayState state;
    IMedia*       media;
    int           lastBrewVol;
    int           lastBrewPan;
    uint8_t*      wavBuf;       // fallback: WAV descompactado tocado via MMD_BUFFER
    uint32_t      wavBufSize;
    RE1PcmSource* pcmSrc;       // streaming: PCM cru sob demanda via ISource
    BOOL          streaming;
    AEEMediaDataEx   mdEx;      // precisa viver enquanto o IMedia existir
    AEEMediaWaveSpec waveSpec;
    char          path[260];
#ifdef AEE_SIMULATOR
    uint8_t*      simBuf;
    uint32_t      simBufSize;
#endif
};

static ZeeboSoundBank s_banks[MAX_BANKS];

static void OnBankMediaNotify(void* pUser, AEEMediaCmdNotify* pn)
{
    ZeeboSoundBank* b = (ZeeboSoundBank*)pUser;
    if (!b || !pn || !b->media || pn->pIMedia != b->media) {
        return;
    }

    switch (pn->nCmd) {
    case MM_CMD_PLAY:
        switch (pn->nStatus) {
        case MM_STATUS_START:
        case MM_STATUS_REPEAT:
        case MM_STATUS_RESUME:
            b->state = BPS_PLAYING;
            b->startMs = plat_time_ms();
            break;
        case MM_STATUS_PAUSE:
            b->state = BPS_PAUSED;
            break;
        case MM_STATUS_DONE:
            b->state = BPS_DONE;
            break;
        case MM_STATUS_ABORT:
            b->state = BPS_ABORTED;
            break;
        default:
            break;
        }
        break;

    case MM_CMD_GETTOTALTIME:
        if (pn->nStatus == MM_STATUS_DONE && pn->pCmdData) {
            DWORD t = (DWORD)(uintptr_t)pn->pCmdData;
            if (t > 0) b->durationMs = t;
        }
        break;

    default:
        break;
    }
}

static int MillibelsToBrewVol(int mB)
{
    if (mB <= -10000) return 0;
    if (mB >= 0) return AEE_MAX_VOLUME;
    double amp = pow(10.0, (double)mB / 2000.0);
    int v = (int)(amp * (double)AEE_MAX_VOLUME + 0.5);
    if (v < 0) v = 0;
    if (v > AEE_MAX_VOLUME) v = AEE_MAX_VOLUME;
    return v;
}

static int DsPanToBrewPan(int dsPan)
{
    // DirectSound pan: -10000 (left) .. 0 (center) .. +10000 (right) -> BREW 0..MM_MAX_PAN (128)
    int p = ((dsPan + 10000) * MM_MAX_PAN) / 20000;
    if (p < 0) p = 0;
    if (p > MM_MAX_PAN) p = MM_MAX_PAN;
    return p;
}

static void StopMediaSafe(IMedia* m)
{
    if (!m || !m->pvt) return;
    boolean changing = FALSE;
    int st = m->pvt->GetState(m, &changing);
    if (st == MM_STATE_PLAY || st == MM_STATE_PLAY_PAUSE || changing) {
        m->pvt->Stop(m);
    }
}

static void OnRe1AudioSuspend(void)
{
    for (int i = 1; i < MAX_BANKS; ++i) {
        if (s_banks[i].active && s_banks[i].media && s_banks[i].media->pvt) {
            StopMediaSafe(s_banks[i].media);
            s_banks[i].state = BPS_IDLE;
        }
    }
}

void plat_audio_probe_and_cache_volume(void)
{
}

void plat_audio_restore_volume(void)
{
}

void InitializeSoundSystem(void)
{
    if (!g_pDirectSound) {
        g_pDirectSound = new DirectSound(NULL);
    }
}

DirectSound::DirectSound(HWND hwnd)
{
    (void)hwnd;
    memset(this, 0, sizeof(*this));
    m_bInitialized = 1;
    // SoundApi.cpp checks offset +0x10 on g_pDirectSound before calling CreateSound/PlaySound.
    *(int*)((BYTE*)this + 0x10) = 1;
    memset(s_banks, 0, sizeof(s_banks));
    g_Zeebo.pfnAudioSuspend = OnRe1AudioSuspend;
}

void DirectSound::compact(void) {}

int DirectSound::GetStatus(int bank)
{
    if (bank <= 0 || bank >= MAX_BANKS || !s_banks[bank].active) {
        return 0;
    }
    ZeeboSoundBank& b = s_banks[bank];
    if (b.state == BPS_IDLE || b.state == BPS_DONE || b.state == BPS_ABORTED) {
        return 0;
    }
    // PCM cru via ISource nao tem fim de stream: quem marca o tempo e' o jogo.
    if (b.streaming && b.durationMs > 0 && b.media && b.media->pvt) {
        DWORD elapsed = plat_time_ms() - b.startMs;
        if (elapsed >= b.durationMs) {
            if (b.loop) {
                RE1PcmSource_Reset(b.pcmSrc);
                StopMediaSafe(b.media);
                b.media->pvt->Play(b.media);
                b.startMs = plat_time_ms();
                return 1;
            }
            StopMediaSafe(b.media);
            b.state = BPS_DONE;
            return 0;
        }
    }
    if (b.media && b.media->pvt) {
        boolean changing = FALSE;
        int st = b.media->pvt->GetState(b.media, &changing);
        if (st == MM_STATE_PLAY || st == MM_STATE_PLAY_PAUSE || changing) {
            return 1;
        }
        if (b.state == BPS_STARTING && (plat_time_ms() - b.cmdMs) < 250u) {
            return 1;
        }
#ifdef AEE_SIMULATOR
        if (b.loop) {
            return 1;
        }
        if (b.durationMs > 0 && (plat_time_ms() - b.startMs) < b.durationMs) {
            return 1;
        }
        b.state = BPS_DONE;
        return 0;
#else
        b.state = BPS_DONE;
        return 0;
#endif
    }
    if (!b.loop && b.durationMs > 0) {
        if ((plat_time_ms() - b.startMs) >= b.durationMs) {
            b.state = BPS_DONE;
            return 0;
        }
    }
    return 1;
}

void DirectSound::Release(void)
{
    if (g_Zeebo.pfnAudioSuspend == OnRe1AudioSuspend) {
        g_Zeebo.pfnAudioSuspend = NULL;
    }
    for (int i = 1; i < MAX_BANKS; ++i) {
        DestroySound(i);
    }
    m_bInitialized = 0;
}

void DirectSound::Reload(void) {}

void DirectSound::DestroySound(int bank)
{
    if (bank > 0 && bank < MAX_BANKS) {
        ZeeboSoundBank& b = s_banks[bank];
        if (b.media && b.media->pvt) {
            b.media->pvt->RegisterNotify(b.media, NULL, NULL);
            StopMediaSafe(b.media);
            b.media->pvt->Release(b.media);
        }
        free(b.wavBuf);
        free(b.pcmSrc);
#ifdef AEE_SIMULATOR
        if (b.simBuf) {
            free(b.simBuf);
        }
#endif
        memset(&b, 0, sizeof(b));
    }
}

int DirectSound::NewDirectSoundBuffer(DWORD* wavBlk)
{
    (void)wavBlk;
    return 0;
}

void DirectSound::StopSound(int bank)
{
    if (bank > 0 && bank < MAX_BANKS) {
        ZeeboSoundBank& b = s_banks[bank];
        b.state = BPS_IDLE;
        StopMediaSafe(b.media);
    }
}

void DirectSound::PlaySound(int bank, unsigned int slot)
{
    if (bank <= 0 || bank >= MAX_BANKS || !s_banks[bank].active) {
        return;
    }
    ZeeboSoundBank& b = s_banks[bank];
    b.slot    = (int)slot;
    b.loop    = (slot != 0) ? TRUE : FALSE;
    b.cmdMs   = plat_time_ms();
    b.startMs = b.cmdMs;
    b.state   = BPS_STARTING;

    if (b.media && b.media->pvt) {
        IMedia* m = b.media;
        StopMediaSafe(m);
        // O ISource de PCM cru e' sequencial: volta ao inicio antes de tocar.
        if (b.streaming && b.pcmSrc) RE1PcmSource_Reset(b.pcmSrc);
        // MM_PARM_PLAY_REPEAT: 0 = loop forever, 1 = play once (default)
        m->pvt->SetMediaParm(m, MM_PARM_PLAY_REPEAT, b.loop ? 0 : 1, 0);

        int bv = MillibelsToBrewVol(b.vol);
        b.lastBrewVol = bv;
        m->pvt->SetMediaParm(m, MM_PARM_VOLUME, bv, 0);

        int bp = DsPanToBrewPan(b.pan);
        b.lastBrewPan = bp;
        if (bp != (MM_MAX_PAN / 2)) {
            m->pvt->SetMediaParm(m, MM_PARM_PAN, bp, 0);
        }

        if (m->pvt->Play(m) != AEE_SUCCESS) {
            b.state = BPS_ABORTED;
        }
    }
}

void DirectSound::SetVol(int bank, int vol)
{
    if (bank > 0 && bank < MAX_BANKS) {
        ZeeboSoundBank& b = s_banks[bank];
        b.vol = vol;
        if (b.media && b.media->pvt) {
            int bv = MillibelsToBrewVol(vol);
            if (bv != b.lastBrewVol) {
                b.lastBrewVol = bv;
                b.media->pvt->SetMediaParm(b.media, MM_PARM_VOLUME, bv, 0);
            }
        }
    }
}

void DirectSound::SetPan(int bank, int pan)
{
    if (bank > 0 && bank < MAX_BANKS) {
        ZeeboSoundBank& b = s_banks[bank];
        b.pan = pan;
        if (b.media && b.media->pvt) {
            int bp = DsPanToBrewPan(pan);
            if (bp != b.lastBrewPan) {
                b.lastBrewPan = bp;
                b.media->pvt->SetMediaParm(b.media, MM_PARM_PAN, bp, 0);
            }
        }
    }
}

int DirectSound::GetVol(int bank)
{
    if (bank > 0 && bank < MAX_BANKS) {
        return s_banks[bank].vol;
    }
    return 0;
}

#ifndef AEE_SIMULATOR
// Tenta tocar o WAV por streaming de PCM cru direto do PAK. Devolve NULL se
// o dispositivo nao implementar MMD_ISOURCE / AEECLSID_MEDIAPCM (fallback).
static IMedia* CreateStreamingMedia(ZeeboSoundBank& b)
{
    if (!RE1Arc_IsOpen() || !g_Zeebo.pShell || !g_Zeebo.pShell->pvt) return NULL;
    const RE1ArcEntry* e = RE1Arc_Find(b.path);
    if (!e) return NULL;

    RE1PcmSource* src = (RE1PcmSource*)malloc(sizeof(RE1PcmSource));
    if (!src) return NULL;
    if (!RE1PcmSource_Init(src, e)) {
        free(src);
        return NULL;
    }

    IMedia* m = NULL;
    if (ISHELL_CreateInstance(g_Zeebo.pShell, AEECLSID_MEDIAPCM, (void**)&m) != AEE_SUCCESS ||
        !m || !m->pvt) {
        if (m && m->pvt) m->pvt->Release(m);
        free(src);
        return NULL;
    }

    RE1Pcm_FillWaveSpec(&b.waveSpec, &src->info);
    RE1Pcm_FillMediaData(&b.mdEx, RE1PcmSource_AsISource(src), &b.waveSpec, 8192u);
    if (m->pvt->SetMediaParm(m, MM_PARM_MEDIA_DATA,
                             (int32_t)&b.mdEx, 1) != AEE_SUCCESS) {
        m->pvt->Release(m);
        free(src);
        return NULL;
    }

    b.pcmSrc = src;
    b.streaming = TRUE;
    uint64_t rate = b.waveSpec.dwBytesPerSec ? b.waveSpec.dwBytesPerSec : 44100u;
    b.durationMs = (DWORD)(((uint64_t)src->info.pcmDataBytes * 1000ULL) / rate);
    Zeebo_Log("[AUD] streaming '%s': %u ch %u Hz %u bytes PCM\n",
              b.path, src->info.channels, src->info.sampleRate, src->info.pcmDataBytes);
    return m;
}
#endif

int DirectSound::CreateSound(const char* wavName)
{
    if (!wavName) return 0;

    for (int i = 1; i < MAX_BANKS; ++i) {
        if (!s_banks[i].active) {
            ZeeboSoundBank& b = s_banks[i];
            memset(&b, 0, sizeof(b));
            b.active      = TRUE;
            b.durationMs  = 250;
            b.lastBrewVol = -1;
            b.lastBrewPan = -1;
            plat_normalize_path(wavName, b.path, sizeof(b.path));
            if (g_Zeebo.pFileMgr && g_Zeebo.pFileMgr->pvt &&
                strncmp(b.path, "assets/", 7) == 0 &&
                g_Zeebo.pFileMgr->pvt->Test(g_Zeebo.pFileMgr, b.path) != AEE_SUCCESS) {
                memmove(b.path, b.path + 7, strlen(b.path + 7) + 1);
            }

#ifndef AEE_SIMULATOR
            // 1a escolha: streaming de PCM cru direto do PAK (~8 KB por voz).
            IMedia* sm = CreateStreamingMedia(b);
            if (sm) {
                b.media = sm;
                sm->pvt->RegisterNotify(sm, OnBankMediaNotify, &b);
            } else
#endif
            // Fallback: WAV descompactado em RAM via MMD_BUFFER.
            if (g_Zeebo.pMediaUtil && g_Zeebo.pMediaUtil->pvt) {
                size_t wavSize = 0;
                uint8_t* wav = (uint8_t*)plat_file_read_all(b.path, &wavSize);
                AEEMediaData md;
                md.clsData = MMD_BUFFER;
                md.pData   = wav;
                md.dwSize  = (uint32_t)wavSize;
                IMedia* m = NULL;
                int rc = (wav && wavSize > 44)
                    ? g_Zeebo.pMediaUtil->pvt->CreateMedia(g_Zeebo.pMediaUtil, &md, &m)
                    : EFAILED;
                if (rc == AEE_SUCCESS && m && m->pvt) {
                    b.media      = m;
                    b.wavBuf     = wav;
                    b.wavBufSize = (uint32_t)wavSize;
                    m->pvt->RegisterNotify(m, OnBankMediaNotify, &b);
                    int totalMs = m->pvt->GetTotalTime(m);
                    if (totalMs > 0) {
                        b.durationMs = (DWORD)totalMs;
                    }
                } else {
                    if (m && m->pvt) m->pvt->Release(m);
                    free(wav);
                }
            }
            return i;
        }
    }
    return 0;
}

void DirectSound::ErrorRoutine(int code)
{
    (void)code;
}
