// audio.cpp - Zeebo / BREW IMedia audio backend & DirectSound bank manager.
//
// Uses BREW's IMediaUtil + IMedia (AEECLSID_MEDIAUTIL) to play WAV banks
// through the console / zeebx-emu audio mixer.
#include "../platform.h"
#include "zeebo_brew.h"

#include "../../Globals.h"
#include "../../marni/MarniSound.h"
#include "../../system/AssetPath.h"
#include "../../system/AudioFile.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BANKS 81

struct ZeeboSoundBank {
    BOOL    active;
    BOOL    playing;
    BOOL    loop;
    int     vol;        // DirectSound millibels (0 = full, -10000 = silent)
    int     pan;
    int     slot;
    DWORD   startMs;
    DWORD   durationMs;
    IMedia* media;
    char    path[260];
};

static ZeeboSoundBank s_banks[MAX_BANKS];

static int MillibelsToBrewVol(int mB)
{
    if (mB <= -10000) return 0;
    if (mB >= 0) return 100;
    double amp = pow(10.0, (double)mB / 2000.0);
    int v = (int)(amp * 100.0 + 0.5);
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    return v;
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
}

void DirectSound::compact(void) {}

int DirectSound::GetStatus(int bank)
{
    if (bank <= 0 || bank >= MAX_BANKS || !s_banks[bank].active) {
        return 0;
    }
    if (s_banks[bank].media && s_banks[bank].media->pvt) {
        boolean changing = FALSE;
        int st = s_banks[bank].media->pvt->GetState(s_banks[bank].media, &changing);
        return (st == (int)MM_STATE_PLAY) ? 1 : 0;
    }
    if (!s_banks[bank].playing) {
        return 0;
    }
    if (!s_banks[bank].loop && s_banks[bank].durationMs > 0) {
        DWORD elapsed = plat_time_ms() - s_banks[bank].startMs;
        if (elapsed >= s_banks[bank].durationMs) {
            s_banks[bank].playing = FALSE;
            return 0;
        }
    }
    return 1;
}

void DirectSound::Release(void)
{
    for (int i = 1; i < MAX_BANKS; ++i) {
        DestroySound(i);
    }
    m_bInitialized = 0;
}

void DirectSound::Reload(void) {}

void DirectSound::DestroySound(int bank)
{
    if (bank > 0 && bank < MAX_BANKS) {
        if (s_banks[bank].media && s_banks[bank].media->pvt) {
            s_banks[bank].media->pvt->Stop(s_banks[bank].media);
            s_banks[bank].media->pvt->Release(s_banks[bank].media);
        }
        memset(&s_banks[bank], 0, sizeof(s_banks[bank]));
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
        s_banks[bank].playing = FALSE;
        if (s_banks[bank].media && s_banks[bank].media->pvt) {
            s_banks[bank].media->pvt->Stop(s_banks[bank].media);
        }
    }
}

void DirectSound::PlaySound(int bank, unsigned int slot)
{
    if (bank <= 0 || bank >= MAX_BANKS || !s_banks[bank].active) {
        return;
    }
    s_banks[bank].slot    = (int)slot;
    s_banks[bank].loop    = (slot != 0) ? TRUE : FALSE;
    s_banks[bank].playing = TRUE;
    s_banks[bank].startMs = plat_time_ms();

    if (s_banks[bank].media && s_banks[bank].media->pvt) {
        IMedia* m = s_banks[bank].media;
        Zeebo_Log("[SND] PlaySound bank=%d media=%p\n", bank, m);
        m->pvt->Stop(m);
        // MM_PARM_PLAY_REPEAT: 0 = loop forever, 1 = play once
        m->pvt->SetMediaParm(m, (int)MM_PARM_PLAY_REPEAT, s_banks[bank].loop ? 0 : 1, 0);
        m->pvt->SetMediaParm(m, (int)MM_PARM_VOLUME, MillibelsToBrewVol(s_banks[bank].vol), 0);
        m->pvt->Play(m);
        Zeebo_Log("[SND] PlaySound bank=%d done\n", bank);
    }
}

void DirectSound::SetVol(int bank, int vol)
{
    if (bank > 0 && bank < MAX_BANKS) {
        s_banks[bank].vol = vol;
        if (s_banks[bank].media && s_banks[bank].media->pvt) {
            s_banks[bank].media->pvt->SetMediaParm(
                s_banks[bank].media, (int)MM_PARM_VOLUME, MillibelsToBrewVol(vol), 0);
        }
    }
}

void DirectSound::SetPan(int bank, int pan)
{
    if (bank > 0 && bank < MAX_BANKS) {
        s_banks[bank].pan = pan;
    }
}

int DirectSound::GetVol(int bank)
{
    if (bank > 0 && bank < MAX_BANKS) {
        return s_banks[bank].vol;
    }
    return 0;
}

int DirectSound::CreateSound(const char* wavName)
{
    if (!wavName) return 0;

    for (int i = 1; i < MAX_BANKS; ++i) {
        if (!s_banks[i].active) {
            memset(&s_banks[i], 0, sizeof(s_banks[i]));
            s_banks[i].active     = TRUE;
            s_banks[i].durationMs = 250;
            plat_normalize_path(wavName, s_banks[i].path, sizeof(s_banks[i].path));
            if (g_Zeebo.pFileMgr && g_Zeebo.pFileMgr->pvt &&
                strncmp(s_banks[i].path, "assets/", 7) == 0 &&
                g_Zeebo.pFileMgr->pvt->Test(g_Zeebo.pFileMgr, s_banks[i].path) != AEE_SUCCESS) {
                memmove(s_banks[i].path, s_banks[i].path + 7, strlen(s_banks[i].path + 7) + 1);
            }

            if (g_Zeebo.pMediaUtil && g_Zeebo.pMediaUtil->pvt) {
                AEEMediaData md;
                md.clsData = MMD_FILE_NAME;
                md.pData   = s_banks[i].path;
                md.dwSize  = 0;
                IMedia* m = NULL;
                int rc = g_Zeebo.pMediaUtil->pvt->CreateMedia(g_Zeebo.pMediaUtil, &md, &m);
                static int s_sndLog = 0;
                if (s_sndLog < 15) {
                    ++s_sndLog;
                    Zeebo_Log("[SND] CreateSound '%s' -> rc=%d media=%p\n", s_banks[i].path, rc, m);
                }
                if (rc == AEE_SUCCESS && m) {
                    s_banks[i].media = m;
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
