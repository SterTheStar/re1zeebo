// avi_demux.cpp - Streaming RIFF/AVI demuxer for Zeebo (uses IFile, not 55MB RAM).
#include "avi_demux.h"
#include "zeebo_brew.h"
#include "re1_lz4.h"
#include "re1_archive.h"
#include "../platform.h"

#include <stdlib.h>
#include <string.h>

#define FOURCC(a, b, c, d) \
    ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | \
     ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24))

struct AviDemux {
    IFile*              file;   /* NULL quando veio do PAK */
    const RE1ArcEntry*  entry;  /* NULL quando veio de arquivo solto */
    uint32_t     fileSize;
    uint32_t     moviStart;
    uint32_t     moviEnd;
    uint32_t     cursor;
    uint8_t*     frameBuf;
    uint32_t     frameBufCap;
    AviVideoInfo info;
};

static uint16_t ReadU16LE(const uint8_t* p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t ReadU32LE(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int FileReadAt(IFile* f, uint32_t offset, void* dst, uint32_t len)
{
    if (!f || !f->pvt) return 0;
    f->pvt->Seek(f, _SEEK_START, (int32_t)offset);
    uint32_t total = 0;
    while (total < len) {
        int32_t n = f->pvt->Read(f, (uint8_t*)dst + total, len - total);
        if (n <= 0) break;
        total += (uint32_t)n;
    }
    return (total == len) ? 1 : 0;
}

static int ReadAt(AviDemux* avi, uint32_t offset, void* dst, uint32_t len)
{
    if (avi->entry) return RE1Arc_ReadAt(avi->entry, offset, dst, len);
    return ReadAt(avi, offset, dst, len);
}

// Cada chunk '00dc'/'01wb' pode ter o payload embrulhado em R1LZ. O tamanho
// declarado no AVI e' o tamanho EMPACOTADO; todas as contagens de amostras e
// o tamanho devolvido ao Cinepak precisam usar o tamanho original.
static int R1OriginalSize(AviDemux* avi, uint32_t payload, uint32_t stored)
{
    uint8_t h[RE1LZ_ADPCM_HEADER_SIZE];
    if (stored >= sizeof(h) &&
        ReadAt(avi, payload, h, sizeof(h))) {
        RE1LZInfo info;
        if (RE1LZ_ParseHeader(h, sizeof(h), &info) == 1 &&
            info.originalSize <= RE1LZ_MAX_ORIGINAL) {
            return (int)info.originalSize;
        }
    }
    return (int)stored;
}

// Le um chunk e devolve um buffer malloc'ado com o payload original.
static uint8_t* ReadChunkDecoded(AviDemux* avi, uint32_t payload,
                                 uint32_t stored, uint32_t* outLen)
{
    *outLen = 0;
    uint8_t h[RE1LZ_ADPCM_HEADER_SIZE];
    if (stored >= sizeof(h) &&
        ReadAt(avi, payload, h, sizeof(h))) {
        RE1LZInfo info;
        if (RE1LZ_ParseHeader(h, sizeof(h), &info) == 1) {
            if (info.flags == RE1LZ_FLAG_ADPCM) {
                // Audio em IMA ADPCM: RE1LZ_Unpack devolve PCM pronto.
                uint8_t* whole = (uint8_t*)malloc(stored ? stored : 1u);
                if (!whole) return NULL;
                memcpy(whole, h, sizeof(h));
                if (!ReadAt(avi, payload + sizeof(h),
                                whole + sizeof(h), stored - sizeof(h))) {
                    free(whole);
                    return NULL;
                }
                void* out = NULL;
                size_t n = 0;
                int rc = RE1LZ_Unpack(whole, stored, &out, &n);
                free(whole);
                if (rc != 1) return NULL;
                *outLen = (uint32_t)n;
                return (uint8_t*)out;
            }
            if (info.compressedSize > stored - RE1LZ_HEADER_SIZE) return NULL;
            uint8_t* tmp = (uint8_t*)malloc(info.compressedSize ? info.compressedSize : 1u);
            if (!tmp) return NULL;
            if (!ReadAt(avi, payload + RE1LZ_HEADER_SIZE,
                            tmp, info.compressedSize)) {
                free(tmp);
                return NULL;
            }
            uint8_t* dst = (uint8_t*)malloc(info.originalSize);
            if (!dst) {
                free(tmp);
                return NULL;
            }
            int n = RE1LZ_DecompressBlock(tmp, info.compressedSize,
                                          dst, info.originalSize);
            free(tmp);
            if (n != (int)info.originalSize ||
                RE1LZ_Crc32(dst, info.originalSize) != info.crc32) {
                free(dst);
                return NULL;
            }
            *outLen = info.originalSize;
            return dst;
        }
    }

    uint8_t* raw = (uint8_t*)malloc(stored ? stored : 1u);
    if (!raw) return NULL;
    if (!ReadAt(avi, payload, raw, stored)) {
        free(raw);
        return NULL;
    }
    *outLen = stored;
    return raw;
}

static void ParseAviHeaders(AviDemux* avi)
{
    uint8_t hdr[64];
    if (avi->fileSize < 12 || !ReadAt(avi, 0, hdr, 12)) {
        return;
    }
    if (ReadU32LE(hdr) != FOURCC('R', 'I', 'F', 'F') ||
        ReadU32LE(hdr + 8) != FOURCC('A', 'V', 'I', ' ')) {
        return;
    }

    uint32_t pos = 12;
    int inAudioStream = 0;
    while (pos + 8 <= avi->fileSize) {
        uint8_t ch[12];
        if (!ReadAt(avi, pos, ch, 8)) break;

        uint32_t tag = ReadU32LE(ch);
        uint32_t chunkSize = ReadU32LE(ch + 4);
        uint32_t payload = pos + 8;
        if (payload + chunkSize > avi->fileSize) break;

        if (tag == FOURCC('L', 'I', 'S', 'T') && chunkSize >= 4) {
            if (!ReadAt(avi, payload, ch, 4)) break;
            uint32_t listType = ReadU32LE(ch);
            if (listType == FOURCC('m', 'o', 'v', 'i')) {
                avi->moviStart = payload + 4;
                avi->moviEnd   = payload + chunkSize;
                break;
            }
            if (listType == FOURCC('h', 'd', 'r', 'l') ||
                listType == FOURCC('s', 't', 'r', 'l')) {
                if (listType == FOURCC('s', 't', 'r', 'l')) inAudioStream = 0;
                pos = payload + 4;
                continue;
            }
        } else if (tag == FOURCC('a', 'v', 'i', 'h') && chunkSize >= 40) {
            if (ReadAt(avi, payload, hdr, 40)) {
                uint32_t usPerFrame = ReadU32LE(hdr + 0);
                uint32_t total      = ReadU32LE(hdr + 16);
                uint32_t w          = ReadU32LE(hdr + 32);
                uint32_t h          = ReadU32LE(hdr + 36);
                avi->info.width       = (int)w;
                avi->info.height      = (int)h;
                avi->info.totalFrames = total;
                avi->info.fps = (usPerFrame > 0) ? (1000000.0 / (double)usPerFrame) : 10.0;
            }
        } else if (tag == FOURCC('s', 't', 'r', 'h') && chunkSize >= 8) {
            if (ReadAt(avi, payload, hdr, 8)) {
                uint32_t fccType    = ReadU32LE(hdr + 0);
                uint32_t fccHandler = ReadU32LE(hdr + 4);
                if (fccType == FOURCC('v', 'i', 'd', 's')) {
                    avi->info.fourcc = fccHandler;
                    inAudioStream = 0;
                } else if (fccType == FOURCC('a', 'u', 'd', 's')) {
                    inAudioStream = 1;
                }
            }
        } else if (tag == FOURCC('s', 't', 'r', 'f') && inAudioStream && chunkSize >= 16) {
            if (ReadAt(avi, payload, hdr, 16)) {
                avi->info.audioFormat         = ReadU16LE(hdr + 0);
                avi->info.audioChannels       = ReadU16LE(hdr + 2);
                avi->info.audioSampleRate     = ReadU32LE(hdr + 4);
                avi->info.audioAvgBytesPerSec = ReadU32LE(hdr + 8);
                avi->info.audioBlockAlign     = ReadU16LE(hdr + 12);
                avi->info.audioBitsPerSample  = ReadU16LE(hdr + 14);
            }
        }

        pos = payload + ((chunkSize + 1u) & ~1u);
    }

    avi->cursor = avi->moviStart;
}

AviDemux* AviDemux_Open(const char* path)
{
    if (!path) return NULL;

    char norm[260];
    plat_normalize_path(path, norm, sizeof(norm));

    const RE1ArcEntry* entry = RE1Arc_IsOpen() ? RE1Arc_Find(path) : NULL;
    IFile* f = NULL;
    uint32_t fileSize = 0;

    if (entry) {
        fileSize = entry->size;
    } else {
        if (!g_Zeebo.pFileMgr || !g_Zeebo.pFileMgr->pvt) return NULL;
        if (strncmp(norm, "assets/", 7) == 0 &&
            g_Zeebo.pFileMgr->pvt->Test(g_Zeebo.pFileMgr, norm) != AEE_SUCCESS) {
            memmove(norm, norm + 7, strlen(norm + 7) + 1);
        }
        f = g_Zeebo.pFileMgr->pvt->OpenFile(g_Zeebo.pFileMgr, norm, _OFM_READ);
        if (!f) {
            Zeebo_Log("[VID] AviDemux_Open '%s' -> NOT FOUND\n", norm);
            return NULL;
        }
        AEEFileInfo info;
        memset(&info, 0, sizeof(info));
        if (f->pvt->GetInfo(f, &info) != AEE_SUCCESS || info.dwSize < 64) {
            f->pvt->Release(f);
            return NULL;
        }
        fileSize = info.dwSize;
    }

    AviDemux* avi = (AviDemux*)calloc(1, sizeof(AviDemux));
    if (!avi) {
        if (f) f->pvt->Release(f);
        return NULL;
    }

    avi->file     = f;
    avi->entry    = entry;
    avi->fileSize = fileSize;
    avi->info.fps = 10.0;

    ParseAviHeaders(avi);
    Zeebo_Log("[VID] AviDemux_Open '%s' %dx%d fps=%.2f frames=%u\n",
              norm, avi->info.width, avi->info.height, avi->info.fps, (unsigned)avi->info.totalFrames);

    if (avi->moviStart == 0 || avi->info.width <= 0 || avi->info.height <= 0) {
        AviDemux_Close(avi);
        return NULL;
    }

    return avi;
}

void AviDemux_Close(AviDemux* avi)
{
    if (!avi) return;
    if (avi->file && avi->file->pvt) {
        avi->file->pvt->Release(avi->file);
    }
    free(avi->frameBuf);
    free(avi);
}

const AviVideoInfo* AviDemux_GetInfo(const AviDemux* avi)
{
    return avi ? &avi->info : NULL;
}

int AviDemux_GetDurationMs(const AviDemux* avi)
{
    if (!avi || avi->info.fps <= 0.0 || avi->info.totalFrames == 0) {
        return 0;
    }
    return (int)((double)avi->info.totalFrames / avi->info.fps * 1000.0);
}

void AviDemux_Rewind(AviDemux* avi)
{
    if (avi) {
        avi->cursor = avi->moviStart;
    }
}

int AviDemux_NextVideoFrame(AviDemux* avi, const uint8_t** outData, uint32_t* outSize)
{
    if (!avi || !outData || !outSize) return 0;

    while (avi->cursor + 8 <= avi->moviEnd) {
        uint8_t ch[8];
        if (!ReadAt(avi, avi->cursor, ch, 8)) break;

        uint32_t tag  = ReadU32LE(ch);
        uint32_t size = ReadU32LE(ch + 4);
        uint32_t payload = avi->cursor + 8;
        if (payload + size > avi->moviEnd) break;

        avi->cursor = payload + ((size + 1u) & ~1u);

        if (tag == FOURCC('L', 'I', 'S', 'T') && size >= 4) {
            avi->cursor = payload + 4;
            continue;
        }

        if (tag == FOURCC('0', '0', 'd', 'c') || tag == FOURCC('0', '0', 'd', 'b')) {
            if (size > 0) {
                uint32_t original = (uint32_t)R1OriginalSize(avi, payload, size);
                if (original > avi->frameBufCap) {
                    uint8_t* nb = (uint8_t*)realloc(avi->frameBuf, original);
                    if (!nb) return 0;
                    avi->frameBuf    = nb;
                    avi->frameBufCap = original;
                }
                uint32_t got = 0;
                uint8_t* decoded = ReadChunkDecoded(avi, payload, size, &got);
                if (!decoded || got != original) {
                    free(decoded);
                    return 0;
                }
                memcpy(avi->frameBuf, decoded, got);
                free(decoded);
                size = original;
            }
            *outData = avi->frameBuf;
            *outSize = size;
            return 1;
        }
    }

    return 0;
}

// AviDemux_ExtractWav removido: montar o WAV PCM inteiro (ate ~20 MB)
// estouraria os 32 MB de RAM do Zeebo. Audio de FMV = <FILME>.AUDIO + ISource.
