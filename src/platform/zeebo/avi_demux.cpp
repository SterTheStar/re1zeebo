// avi_demux.cpp - Streaming RIFF/AVI demuxer for Zeebo (uses IFile, not 55MB RAM).
#include "avi_demux.h"
#include "zeebo_brew.h"
#include "../platform.h"

#include <stdlib.h>
#include <string.h>

#define FOURCC(a, b, c, d) \
    ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | \
     ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24))

struct AviDemux {
    IFile*       file;
    uint32_t     fileSize;
    uint32_t     moviStart;
    uint32_t     moviEnd;
    uint32_t     cursor;
    uint8_t*     frameBuf;
    uint32_t     frameBufCap;
    AviVideoInfo info;
};

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

static void ParseAviHeaders(AviDemux* avi)
{
    uint8_t hdr[64];
    if (avi->fileSize < 12 || !FileReadAt(avi->file, 0, hdr, 12)) {
        return;
    }
    if (ReadU32LE(hdr) != FOURCC('R', 'I', 'F', 'F') ||
        ReadU32LE(hdr + 8) != FOURCC('A', 'V', 'I', ' ')) {
        return;
    }

    uint32_t pos = 12;
    while (pos + 8 <= avi->fileSize) {
        uint8_t ch[12];
        if (!FileReadAt(avi->file, pos, ch, 8)) break;

        uint32_t tag = ReadU32LE(ch);
        uint32_t chunkSize = ReadU32LE(ch + 4);
        uint32_t payload = pos + 8;
        if (payload + chunkSize > avi->fileSize) break;

        if (tag == FOURCC('L', 'I', 'S', 'T') && chunkSize >= 4) {
            if (!FileReadAt(avi->file, payload, ch, 4)) break;
            uint32_t listType = ReadU32LE(ch);
            if (listType == FOURCC('m', 'o', 'v', 'i')) {
                avi->moviStart = payload + 4;
                avi->moviEnd   = payload + chunkSize;
                break;
            }
            if (listType == FOURCC('h', 'd', 'r', 'l') ||
                listType == FOURCC('s', 't', 'r', 'l')) {
                pos = payload + 4;
                continue;
            }
        } else if (tag == FOURCC('a', 'v', 'i', 'h') && chunkSize >= 40) {
            if (FileReadAt(avi->file, payload, hdr, 40)) {
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
            if (FileReadAt(avi->file, payload, hdr, 8)) {
                uint32_t fccType    = ReadU32LE(hdr + 0);
                uint32_t fccHandler = ReadU32LE(hdr + 4);
                if (fccType == FOURCC('v', 'i', 'd', 's')) {
                    avi->info.fourcc = fccHandler;
                }
            }
        }

        pos = payload + ((chunkSize + 1u) & ~1u);
    }

    avi->cursor = avi->moviStart;
}

AviDemux* AviDemux_Open(const char* path)
{
    if (!path || !g_Zeebo.pFileMgr || !g_Zeebo.pFileMgr->pvt) return NULL;

    char norm[260];
    plat_normalize_path(path, norm, sizeof(norm));
    if (strncmp(norm, "assets/", 7) == 0 &&
        g_Zeebo.pFileMgr->pvt->Test(g_Zeebo.pFileMgr, norm) != AEE_SUCCESS) {
        memmove(norm, norm + 7, strlen(norm + 7) + 1);
    }

    IFile* f = g_Zeebo.pFileMgr->pvt->OpenFile(g_Zeebo.pFileMgr, norm, _OFM_READ);
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

    AviDemux* avi = (AviDemux*)calloc(1, sizeof(AviDemux));
    if (!avi) {
        f->pvt->Release(f);
        return NULL;
    }

    avi->file     = f;
    avi->fileSize = info.dwSize;
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
        if (!FileReadAt(avi->file, avi->cursor, ch, 8)) break;

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
                if (size > avi->frameBufCap) {
                    uint8_t* nb = (uint8_t*)realloc(avi->frameBuf, size);
                    if (!nb) return 0;
                    avi->frameBuf    = nb;
                    avi->frameBufCap = size;
                }
                if (!FileReadAt(avi->file, payload, avi->frameBuf, size)) {
                    return 0;
                }
            }
            *outData = avi->frameBuf;
            *outSize = size;
            return 1;
        }
    }

    return 0;
}
