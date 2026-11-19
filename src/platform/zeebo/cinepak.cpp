// cinepak.cpp - Clean-room Cinepak (CVID) decoder outputting RGBA8888.
#include "cinepak.h"

#include <stdlib.h>
#include <string.h>

#define MAX_STRIPS 32

typedef struct CinepakCodebook {
    uint8_t y0, y1, y2, y3;
    int8_t  u, v;
} CinepakCodebook;

typedef struct CinepakStrip {
    CinepakCodebook v4[256];
    CinepakCodebook v1[256];
} CinepakStrip;

struct CinepakDecoder {
    int          width;
    int          height;
    uint8_t*     rgba;
    CinepakStrip strips[MAX_STRIPS];
};

static uint16_t ReadU16BE(const uint8_t* p)
{
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

static uint32_t ReadU24BE(const uint8_t* p)
{
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
}

static uint32_t ReadU32BE(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static uint8_t ClampU8(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

static void WritePixelYUV(uint8_t* dst, uint8_t y, int8_t u, int8_t v)
{
    int c = (int)y;
    int d = (int)u;
    int e = (int)v;
    dst[0] = ClampU8(c + ((e * 359) >> 8));
    dst[1] = ClampU8(c - ((d * 88 + e * 183) >> 8));
    dst[2] = ClampU8(c + ((d * 454) >> 8));
    dst[3] = 255;
}

static void DrawBlockV1(CinepakDecoder* dec, int x, int y, const CinepakCodebook* cb)
{
    if (x + 4 > dec->width || y + 4 > dec->height) return;
    int stride = dec->width * 4;
    uint8_t* p = dec->rgba + y * stride + x * 4;

    uint8_t ys[4] = { cb->y0, cb->y1, cb->y2, cb->y3 };
    for (int py = 0; py < 4; ++py) {
        uint8_t* row = p + py * stride;
        uint8_t yL = ys[(py >> 1) * 2 + 0];
        uint8_t yR = ys[(py >> 1) * 2 + 1];
        WritePixelYUV(row + 0,  yL, cb->u, cb->v);
        WritePixelYUV(row + 4,  yL, cb->u, cb->v);
        WritePixelYUV(row + 8,  yR, cb->u, cb->v);
        WritePixelYUV(row + 12, yR, cb->u, cb->v);
    }
}

static void DrawBlockV4(CinepakDecoder* dec, int x, int y,
                        const CinepakCodebook* cb0, const CinepakCodebook* cb1,
                        const CinepakCodebook* cb2, const CinepakCodebook* cb3)
{
    if (x + 4 > dec->width || y + 4 > dec->height) return;
    int stride = dec->width * 4;
    uint8_t* p = dec->rgba + y * stride + x * 4;

    const CinepakCodebook* q[4] = { cb0, cb1, cb2, cb3 };
    for (int qi = 0; qi < 4; ++qi) {
        int ox = (qi & 1) * 2;
        int oy = (qi >> 1) * 2;
        const CinepakCodebook* c = q[qi];
        uint8_t* r0 = p + (oy + 0) * stride + ox * 4;
        uint8_t* r1 = p + (oy + 1) * stride + ox * 4;
        WritePixelYUV(r0 + 0, c->y0, c->u, c->v);
        WritePixelYUV(r0 + 4, c->y1, c->u, c->v);
        WritePixelYUV(r1 + 0, c->y2, c->u, c->v);
        WritePixelYUV(r1 + 4, c->y3, c->u, c->v);
    }
}

static void LoadCodebookFull(CinepakCodebook* table, const uint8_t* data, uint32_t size)
{
    uint32_t count = size / 6;
    if (count > 256) count = 256;
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* e = data + i * 6;
        table[i].y0 = e[0];
        table[i].y1 = e[1];
        table[i].y2 = e[2];
        table[i].y3 = e[3];
        table[i].u  = (int8_t)e[4];
        table[i].v  = (int8_t)e[5];
    }
}

static void LoadCodebookPartial(CinepakCodebook* table, const uint8_t* data, uint32_t size)
{
    uint32_t pos = 0;
    uint32_t idx = 0;
    while (pos + 4 <= size && idx < 256) {
        uint32_t mask = ReadU32BE(data + pos);
        pos += 4;
        for (int bit = 31; bit >= 0 && idx < 256; --bit, ++idx) {
            if (mask & (1u << bit)) {
                if (pos + 6 > size) return;
                const uint8_t* e = data + pos;
                table[idx].y0 = e[0];
                table[idx].y1 = e[1];
                table[idx].y2 = e[2];
                table[idx].y3 = e[3];
                table[idx].u  = (int8_t)e[4];
                table[idx].v  = (int8_t)e[5];
                pos += 6;
            }
        }
    }
}

static void DecodeVectors(CinepakDecoder* dec, CinepakStrip* strip,
                          uint16_t chunkId, const uint8_t* data, uint32_t size,
                          int x0, int y0, int x1, int y1)
{
    uint32_t pos = 0;
    uint32_t flagMask = 0;
    int bitsLeft = 0;

    for (int y = y0; y < y1; y += 4) {
        for (int x = x0; x < x1; x += 4) {
            if (chunkId == 0x3200) {
                if (pos >= size) return;
                DrawBlockV1(dec, x, y, &strip->v1[data[pos++]]);
                continue;
            }

            if (chunkId == 0x3100) {
                if (bitsLeft == 0) {
                    if (pos + 4 > size) return;
                    flagMask = ReadU32BE(data + pos);
                    pos += 4;
                    bitsLeft = 32;
                }
                int skip = !(flagMask & 0x80000000u);
                flagMask <<= 1;
                --bitsLeft;
                if (skip) continue;
            }

            if (bitsLeft == 0) {
                if (pos + 4 > size) return;
                flagMask = ReadU32BE(data + pos);
                pos += 4;
                bitsLeft = 32;
            }
            int isV4 = (flagMask & 0x80000000u) != 0;
            flagMask <<= 1;
            --bitsLeft;

            if (isV4) {
                if (pos + 4 > size) return;
                DrawBlockV4(dec, x, y,
                            &strip->v4[data[pos + 0]],
                            &strip->v4[data[pos + 1]],
                            &strip->v4[data[pos + 2]],
                            &strip->v4[data[pos + 3]]);
                pos += 4;
            } else {
                if (pos >= size) return;
                DrawBlockV1(dec, x, y, &strip->v1[data[pos++]]);
            }
        }
    }
}

CinepakDecoder* Cinepak_Create(int width, int height)
{
    if (width <= 0 || height <= 0) return NULL;
    CinepakDecoder* dec = (CinepakDecoder*)calloc(1, sizeof(CinepakDecoder));
    if (!dec) return NULL;

    dec->width  = width;
    dec->height = height;
    dec->rgba   = (uint8_t*)calloc((size_t)width * (size_t)height * 4u, 1);
    if (!dec->rgba) {
        free(dec);
        return NULL;
    }
    return dec;
}

void Cinepak_Destroy(CinepakDecoder* dec)
{
    if (!dec) return;
    free(dec->rgba);
    free(dec);
}

int Cinepak_DecodeFrame(CinepakDecoder* dec, const uint8_t* data, uint32_t size)
{
    if (!dec || !data || size < 10) return 0;

    uint8_t  flags     = data[0];
    uint32_t codedSize = ReadU24BE(data + 1);
    uint16_t numStrips = ReadU16BE(data + 8);
    (void)flags;

    if (codedSize > size) codedSize = size;
    if (numStrips > MAX_STRIPS) numStrips = MAX_STRIPS;

    uint32_t pos = 10;
    for (uint16_t s = 0; s < numStrips && pos + 12 <= codedSize; ++s) {
        if (s > 0 && !(flags & 0x01)) {
            dec->strips[s] = dec->strips[s - 1];
        }

        uint16_t stripSize = ReadU16BE(data + pos + 2);
        uint16_t y0        = ReadU16BE(data + pos + 4);
        uint16_t x0        = ReadU16BE(data + pos + 6);
        uint16_t y1        = ReadU16BE(data + pos + 8);
        uint16_t x1        = ReadU16BE(data + pos + 10);

        if (stripSize < 12 || pos + stripSize > codedSize) break;

        uint32_t cpos = pos + 12;
        uint32_t send = pos + stripSize;
        while (cpos + 4 <= send) {
            uint16_t chunkId   = ReadU16BE(data + cpos);
            uint16_t chunkSize = ReadU16BE(data + cpos + 2);
            if (chunkSize < 4 || cpos + chunkSize > send) break;

            const uint8_t* payload = data + cpos + 4;
            uint32_t payloadSize   = chunkSize - 4;

            switch (chunkId) {
            case 0x2000: LoadCodebookFull(dec->strips[s].v4, payload, payloadSize); break;
            case 0x2100: LoadCodebookFull(dec->strips[s].v1, payload, payloadSize); break;
            case 0x2200: LoadCodebookPartial(dec->strips[s].v4, payload, payloadSize); break;
            case 0x2300: LoadCodebookPartial(dec->strips[s].v1, payload, payloadSize); break;
            case 0x3000:
            case 0x3100:
            case 0x3200:
                DecodeVectors(dec, &dec->strips[s], chunkId, payload, payloadSize,
                              (int)x0, (int)y0, (int)x1, (int)y1);
                break;
            default:
                break;
            }
            cpos += chunkSize;
        }
        pos += stripSize;
    }

    return 1;
}

const uint8_t* Cinepak_GetRGBA(const CinepakDecoder* dec)
{
    return dec ? dec->rgba : NULL;
}
