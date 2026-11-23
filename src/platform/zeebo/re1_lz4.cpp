// re1_lz4.cpp - Implementacao do decoder R1LZ / LZ4 block.
#include "re1_lz4.h"

#include <stdlib.h>
#include <string.h>

static uint16_t Rd16LE(const uint8_t* p)
{
    return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}

static uint32_t Rd32LE(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void Wr16LE(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static void Wr32LE(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

int RE1LZ_ParseHeader(const void* data, size_t size, RE1LZInfo* out)
{
    if (!data || size < RE1LZ_HEADER_SIZE) return 0;
    const uint8_t* p = (const uint8_t*)data;
    if (Rd32LE(p) != RE1LZ_MAGIC_U32) return 0;

    RE1LZInfo info;
    memset(&info, 0, sizeof(info));
    info.version        = Rd16LE(p + 4);
    info.flags          = Rd16LE(p + 6);
    info.originalSize   = Rd32LE(p + 8);
    info.compressedSize = Rd32LE(p + 12);
    info.crc32          = Rd32LE(p + 16);

    if (info.version != 1) return -1;
    if (info.flags != RE1LZ_FLAG_LZ4 && info.flags != RE1LZ_FLAG_ADPCM) return -1;

    size_t hdrSize = (info.flags == RE1LZ_FLAG_ADPCM)
                   ? RE1LZ_ADPCM_HEADER_SIZE : RE1LZ_HEADER_SIZE;
    if (size < hdrSize) return -1;
    if (info.flags == RE1LZ_FLAG_ADPCM) {
        info.sampleRate   = Rd32LE(p + 20);
        info.channels     = Rd16LE(p + 24);
        info.bitsPerSample= Rd16LE(p + 26);
        info.pcmDataBytes = Rd32LE(p + 28);
        info.dataOffset   = Rd32LE(p + 32);
        if (info.channels == 0 || info.channels > 2 ||
            info.bitsPerSample != 16 || info.sampleRate == 0 ||
            info.pcmDataBytes == 0 ||
            (info.dataOffset != 0u && info.dataOffset < 44u) ||
            info.dataOffset + info.pcmDataBytes > info.originalSize) return -1;
    }
    if (info.originalSize == 0 || info.originalSize > RE1LZ_MAX_ORIGINAL) return -1;
    if (info.compressedSize == 0) return -1;
    // O chamador pode passar so' o header (ex.: ISource), entao NAO e' aqui
    // que se valida compressedSize contra o tamanho total. RE1LZ_Unpack faz isso.
    if (out) *out = info;
    return 1;
}

// IMA ADPCM (4 bits/amostra), um bloco por canal. Devolve 0 ou -1.
static int DecodeImaAdpcm(const uint8_t* src, size_t srcSize,
                          const RE1LZInfo& info, uint8_t* pcm)
{
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

    const uint32_t channels = info.channels;
    size_t perChan = (size_t)info.pcmDataBytes / (2u * channels);
    size_t blockBytes = 4u + (perChan + 1u) / 2u;
    if (blockBytes * channels > srcSize) return -1;

    for (uint32_t ch = 0; ch < channels; ++ch) {
        const uint8_t* b = src + ch * blockBytes;
        int pred = (int)Rd16LE(b);
        int idx  = b[2];
        if (idx < 0 || idx > 88) return -1;
        for (size_t s = 0; s < perChan; ++s) {
            const uint8_t byte = b[4 + s / 2];
            int nibble = (s & 1u) ? (int)(byte & 0x0Fu) : (int)(byte >> 4);
            int step = kStep[idx];
            int diff = step >> 3;
            if (nibble & 1) diff += step >> 2;
            if (nibble & 2) diff += step >> 1;
            if (nibble & 4) diff += step;
            if (nibble & 8) pred -= diff; else pred += diff;
            if (pred < -32768) pred = -32768;
            if (pred >  32767) pred =  32767;
            idx += kIndex[nibble];
            if (idx < 0) idx = 0;
            if (idx > 88) idx = 88;
            Wr16LE(pcm + (s * channels + ch) * 2u, (uint16_t)(int16_t)pred);
        }
    }
    return 0;
}

int RE1LZ_DecompressBlock(const uint8_t* src, size_t srcSize,
                          uint8_t* dst, size_t dstCap)
{
    if (!src || !dst) return -1;
    size_t ip = 0, op = 0;

    while (ip < srcSize) {
        const uint8_t token = src[ip++];

        /* Literal run. */
        size_t lit = (size_t)(token >> 4);
        if (lit == 15) {
            while (ip < srcSize && src[ip] == 255) {
                lit += 255u;
                ++ip;
            }
            if (ip >= srcSize) return -1;
            lit += src[ip++];
        }
        if (ip + lit > srcSize || op + lit > dstCap) return -1;
        memcpy(dst + op, src + ip, lit);
        ip += lit;
        op += lit;

        if (ip >= srcSize) break; /* ultimo sequence so tem literais. */

        /* Match. */
        if (ip + 2 > srcSize) return -1;
        size_t offset = (size_t)src[ip] | ((size_t)src[ip + 1] << 8);
        ip += 2;
        // LZ4: offset valido e' 1..65535 (o minimo de 4 e' do MATCH, nao
        // do offset). PCM de audio usa muito offset 1 (repetir a amostra).
        if (offset == 0u || offset > op) return -1;

        size_t match = (size_t)(token & 0x0F);
        if (match == 15) {
            while (ip < srcSize && src[ip] == 255) {
                match += 255u;
                ++ip;
            }
            if (ip >= srcSize) return -1;
            match += src[ip++];
        }
        match += 4u;
        if (op + match > dstCap) return -1;

        const uint8_t* s = dst + op - offset;
        /* copia byte a byte de proposito: match pode sobrepor a saida. */
        for (size_t i = 0; i < match; ++i) {
            dst[op++] = s[i];
        }
    }

    return (int)op;
}

uint32_t RE1LZ_Crc32(const void* data, size_t size)
{
    static uint32_t s_table[256];
    static int s_ready = 0;
    if (!s_ready) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            s_table[i] = c;
        }
        s_ready = 1;
    }
    uint32_t crc = 0xFFFFFFFFu;
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < size; ++i) {
        crc = s_table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

int RE1LZ_Unpack(const void* data, size_t size,
                 void** outData, size_t* outSize)
{
    if (!outData || !outSize) return -1;
    *outData = (void*)data;
    *outSize = size;

    RE1LZInfo info;
    int hdr = RE1LZ_ParseHeader(data, size, &info);
    if (hdr == 0) return 0;
    if (hdr < 0) return -1;

    size_t hdrSize = (info.flags == RE1LZ_FLAG_ADPCM)
                   ? RE1LZ_ADPCM_HEADER_SIZE : RE1LZ_HEADER_SIZE;
    if ((size_t)info.compressedSize > size - hdrSize) return -1;

    uint8_t* dst = (uint8_t*)malloc(info.originalSize);
    if (!dst) return -1;

    if (info.flags == RE1LZ_FLAG_LZ4) {
        const uint8_t* payload = (const uint8_t*)data + RE1LZ_HEADER_SIZE;
        int written = RE1LZ_DecompressBlock(payload, info.compressedSize,
                                            dst, info.originalSize);
        if (written != (int)info.originalSize) {
            free(dst);
            return -1;
        }
    } else {
        // ADPCM: copia o cabecalho WAV original (inclui chunks 'smpl' de loop)
        // e substitui somente o PCM pelos nibbles decodificados.
        const uint8_t* payload = (const uint8_t*)data + RE1LZ_ADPCM_HEADER_SIZE;
        size_t trailing = (size_t)info.originalSize - info.dataOffset
                        - info.pcmDataBytes;
        if (info.compressedSize <= info.dataOffset + trailing) {
            free(dst);
            return -1;
        }
        size_t adpcmLen = info.compressedSize - info.dataOffset - trailing;
        memcpy(dst, payload, info.dataOffset);
        if (DecodeImaAdpcm(payload + info.dataOffset, adpcmLen,
                           info, dst + info.dataOffset) < 0) {
            free(dst);
            return -1;
        }
        if (trailing) {
            memcpy(dst + info.dataOffset + info.pcmDataBytes,
                   payload + info.dataOffset + adpcmLen, trailing);
        }
    }

    if (RE1LZ_Crc32(dst, info.originalSize) != info.crc32) {
        free(dst);
        return -1;
    }

    *outData = dst;
    *outSize = info.originalSize;
    return 1;
}
