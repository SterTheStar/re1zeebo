// pcm_source.cpp - ISource de PCM cru a partir de um bloco IMA ADPCM no PAK.
#include "pcm_source.h"
#include "zeebo_brew.h"

#include <stdlib.h>
#include <string.h>

static const int16_t kStep[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
    143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
    494, 544, 598, 658, 724, 796, 875, 963, 1059, 1165, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026,
    4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767
};
static const int8_t kIndex[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8
};

const ISourceVtbl* PcmVTable(void);

static int16_t RdPredLE(const uint8_t* p)
{
    return (int16_t)(uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

// Garante que o byte 'byteIdx' do bloco do canal esteja na janela.
static int EnsureByte(RE1PcmSource* s, uint32_t ch, uint32_t byteIdx, uint8_t* out)
{
    if (byteIdx >= s->blockBytes) return 0;
    uint32_t base = byteIdx & ~(RE1_PCM_WINDOW - 1u);
    if (s->winBase[ch] != base) {
        uint32_t entryOff = RE1LZ_ADPCM_HEADER_SIZE + s->info.dataOffset
                          + ch * s->blockBytes + base;
        uint32_t len = s->blockBytes - base;
        if (len > RE1_PCM_WINDOW) len = RE1_PCM_WINDOW;
        if (!RE1Arc_ReadAt(s->entry, entryOff, s->win[ch], len)) return 0;
        s->winBase[ch] = base;
    }
    *out = s->win[ch][byteIdx - base];
    return 1;
}

static int DecodeNibble(RE1PcmSource* s, uint32_t ch, uint32_t sample, int16_t* out)
{
    uint32_t byteIdx = 4u + sample / 2u;
    uint8_t byte;
    if (!EnsureByte(s, ch, byteIdx, &byte)) return 0;
    int n = (sample & 1u) ? (int)(byte & 0x0Fu) : (int)(byte >> 4);
    int step = kStep[s->idx[ch]];
    int d = step >> 3;
    if (n & 1) d += step >> 2;
    if (n & 2) d += step >> 1;
    if (n & 4) d += step;
    s->pred[ch] += (n & 8) ? -d : d;
    if (s->pred[ch] < -32768) s->pred[ch] = -32768;
    if (s->pred[ch] >  32767) s->pred[ch] =  32767;
    s->idx[ch] += kIndex[n];
    if (s->idx[ch] < 0) s->idx[ch] = 0;
    if (s->idx[ch] > 88) s->idx[ch] = 88;
    *out = (int16_t)s->pred[ch];
    return 1;
}

void RE1PcmSource_Reset(RE1PcmSource* s)
{
    if (!s || !s->entry) return;
    s->posSamples = 0;
    for (uint32_t ch = 0; ch < 2; ++ch) {
        uint8_t hdr[4];
        uint32_t off = RE1LZ_ADPCM_HEADER_SIZE + s->info.dataOffset
                     + ch * s->blockBytes;
        if (ch >= s->info.channels || !RE1Arc_ReadAt(s->entry, off, hdr, 4)) {
            s->pred[ch] = 0;
            s->idx[ch] = 0;
        } else {
            s->pred[ch] = RdPredLE(hdr);
            s->idx[ch] = hdr[2];
            if (s->idx[ch] < 0 || s->idx[ch] > 88) s->idx[ch] = 0;
        }
        s->winBase[ch] = 0xFFFFFFFFu;
    }
}

int RE1PcmSource_Init(RE1PcmSource* s, const RE1ArcEntry* entry)
{
    if (!s || !entry) return 0;
    memset(s, 0, sizeof(*s));

    uint8_t hdr[RE1LZ_ADPCM_HEADER_SIZE];
    if (!RE1Arc_ReadAt(entry, 0, hdr, sizeof(hdr))) return 0;
    if (RE1LZ_ParseHeader(hdr, sizeof(hdr), &s->info) != 1 ||
        s->info.flags != RE1LZ_FLAG_ADPCM || s->info.channels == 0 ||
        s->info.channels > 2) return 0;

    s->samplesPerCh = s->info.pcmDataBytes / (2u * s->info.channels);
    s->blockBytes = 4u + (s->samplesPerCh + 1u) / 2u;
    if (s->samplesPerCh == 0 ||
        s->info.dataOffset + s->blockBytes * s->info.channels > s->info.compressedSize) {
        return 0;
    }

    s->base.pvt = PcmVTable();
    s->refCount = 1;
    s->entry = entry;
    RE1PcmSource_Reset(s);
    return 1;
}

// ---------------------------------------------------------------------------
// ISource
// ---------------------------------------------------------------------------
static uint32_t Pcm_AddRef(ISource* po)
{
    RE1PcmSource* s = (RE1PcmSource*)po;
    return ++s->refCount;
}

static uint32_t Pcm_Release(ISource* po)
{
    RE1PcmSource* s = (RE1PcmSource*)po;
    if (s->refCount > 0) --s->refCount;
    return s->refCount;
}

static int Pcm_QueryInterface(ISource* po, AEEIID id, void** ppNew)
{
    RE1PcmSource* s = (RE1PcmSource*)po;
    if (!ppNew) return EFAILED;
    *ppNew = NULL;
    if (id == AEEIID_ISOURCE || id == 0x01000001u) {
        Pcm_AddRef(po);
        *ppNew = po;
        return AEE_SUCCESS;
    }
    (void)s;
    return ECLASSNOTSUPPORT;
}

static int32_t Pcm_Read(ISource* po, char* pBuf, int32_t cbBuf)
{
    RE1PcmSource* s = (RE1PcmSource*)po;
    if (!pBuf || cbBuf <= 0) return ISOURCE_END;
    int32_t n = RE1PcmSource_ReadPcm(s, pBuf, cbBuf);
    return n > 0 ? n : ISOURCE_END;
}

static void Pcm_Readable(ISource* po, void* pcb)
{
    (void)po;
    if (pcb && g_Zeebo.pShell && g_Zeebo.pShell->pvt && g_Zeebo.pShell->pvt->Resume) {
        g_Zeebo.pShell->pvt->Resume(g_Zeebo.pShell, pcb);
    }
}

const ISourceVtbl* PcmVTable(void)
{
    static const ISourceVtbl vt = {
        Pcm_AddRef,
        Pcm_Release,
        Pcm_QueryInterface,
        Pcm_Read,
        Pcm_Readable
    };
    return &vt;
}

ISource* RE1PcmSource_AsISource(RE1PcmSource* s) { return &s->base; }

int32_t RE1PcmSource_ReadPcm(RE1PcmSource* s, void* dst, int32_t bytes)
{
    if (!s || !s->entry || !dst || bytes <= 0) return ISOURCE_END;
    uint32_t chans = s->info.channels;
    uint32_t align = chans * 2u;
    uint32_t frames = (uint32_t)bytes / align;
    uint32_t maxFrames = s->info.pcmDataBytes / align;
    if (s->posSamples >= maxFrames) return ISOURCE_END;
    if (frames > maxFrames - s->posSamples) frames = maxFrames - s->posSamples;
    if (frames == 0) return ISOURCE_END;

    int16_t* out = (int16_t*)dst;
    for (uint32_t f = 0; f < frames; ++f) {
        for (uint32_t ch = 0; ch < chans; ++ch) {
            int16_t v = 0;
            if (!DecodeNibble(s, ch, s->posSamples + f, &v)) {
                return (int32_t)(f * align);
            }
            out[f * chans + ch] = v;
        }
    }
    s->posSamples += frames;
    return (int32_t)(frames * align);
}

void RE1Pcm_FillWaveSpec(AEEMediaWaveSpec* spec, const RE1LZInfo* info)
{
    memset(spec, 0, sizeof(*spec));
    spec->wSize = (uint16_t)sizeof(*spec);
    spec->clsMedia = AEECLSID_MEDIAPCM;
    spec->wChannels = (uint16_t)info->channels;
    spec->dwSamplesPerSec = info->sampleRate;
    spec->wBitsPerSample = 16;
    spec->bUnsigned = 0;
    spec->dwBytesPerSec = info->sampleRate * info->channels * 2u;
    spec->wBlockSize = (uint16_t)(info->channels * 2u);
    spec->dwExtra = 0;
}

void RE1Pcm_FillMediaData(AEEMediaDataEx* md, ISource* source,
                          AEEMediaWaveSpec* spec, uint32_t bufferSize)
{
    memset(md, 0, sizeof(*md));
    md->clsData = MMD_ISOURCE;
    md->pData = source;
    md->dwSize = 0;
    md->dwStructSize = (uint32_t)sizeof(*md);
    md->dwCaps = 0;
    md->bRaw = 1;
    md->pSpec = spec;
    md->dwSpecSize = (uint32_t)sizeof(*spec);
    md->dwBufferSize = bufferSize;
}
