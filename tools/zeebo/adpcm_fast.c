/* adpcm_fast.c - IMA ADPCM 4-bit encoder/decoder usado pelo re1pack.py.
 *
 * Compilar como DLL e carregada via ctypes; o Python puro e' fallback lento.
 * O formato e' identico ao decoder de src/platform/zeebo/re1_lz4.cpp:
 * um bloco por canal = [predictor i16][step index u8][pad u8] + nibbles.
 *
 *   cl /LD /O2 adpcm_fast.c
 */
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif
#include <stdint.h>
#include <stddef.h>

static const int16_t STEP[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
    143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
    494, 544, 598, 658, 724, 796, 875, 963, 1059, 1165, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026,
    4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767
};
static const int8_t INDEX[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8
};

EXPORT int re1_adpcm_block_bytes(int per_channel)
{
    return 4 + (per_channel + 1) / 2;
}

EXPORT void re1_adpcm_encode(const int16_t* pcm, int per_channel, int channels,
                             uint8_t* out)
{
    int block = re1_adpcm_block_bytes(per_channel);
    for (int ch = 0; ch < channels; ++ch) {
        uint8_t* b = out + ch * block;
        int pred = 0, idx = 0;
        b[0] = 0; b[1] = 0; b[2] = 0; b[3] = 0;
        for (int i = 0; i < per_channel; ++i) {
            int s = pcm[(int)((size_t)i * channels + ch)];
            int step = STEP[idx];
            int diff = s - pred;
            int absd = diff < 0 ? -diff : diff;
            int n = 0;
            if (absd >= step)       { n |= 4; absd -= step; }
            if (absd >= (step >> 1)) { n |= 2; absd -= step >> 1; }
            if (absd >= (step >> 2)) { n |= 1; }
            if (diff < 0) n |= 8;
            int d = step >> 3;
            if (n & 1) d += step >> 2;
            if (n & 2) d += step >> 1;
            if (n & 4) d += step;
            pred += (n & 8) ? -d : d;
            if (pred < -32768) pred = -32768;
            if (pred >  32767) pred =  32767;
            idx += INDEX[n];
            if (idx < 0) idx = 0;
            if (idx > 88) idx = 88;
            uint8_t* dst = b + 4 + i / 2;
            if (i & 1) *dst = (uint8_t)(*dst | (uint8_t)n);
            else       *dst = (uint8_t)(n << 4);
        }
    }
}

EXPORT void re1_adpcm_decode(const uint8_t* in, int per_channel, int channels,
                             int16_t* pcm)
{
    int block = re1_adpcm_block_bytes(per_channel);
    for (int ch = 0; ch < channels; ++ch) {
        const uint8_t* b = in + ch * block;
        int pred = (int)(int16_t)(uint16_t)(b[0] | (b[1] << 8));
        int idx = b[2];
        if (idx < 0 || idx > 88) idx = 0;
        for (int i = 0; i < per_channel; ++i) {
            uint8_t byte = b[4 + i / 2];
            int n = (i & 1) ? (byte & 0x0F) : (byte >> 4);
            int step = STEP[idx];
            int d = step >> 3;
            if (n & 1) d += step >> 2;
            if (n & 2) d += step >> 1;
            if (n & 4) d += step;
            pred += (n & 8) ? -d : d;
            if (pred < -32768) pred = -32768;
            if (pred >  32767) pred =  32767;
            idx += INDEX[n];
            if (idx < 0) idx = 0;
            if (idx > 88) idx = 88;
            pcm[(int)((size_t)i * channels + ch)] = (int16_t)pred;
        }
    }
}
