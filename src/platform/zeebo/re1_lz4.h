// re1_lz4.h - Container "R1LZ" + decoder LZ4 block para assets do Zeebo.
//
// Formato (little-endian, 20 bytes de cabecalho):
//   +0  'R','1','L','Z'      magic
//   +4  u16 version = 1
//   +6  u16 flags   = 1 (LZ4 cru) ou 2 (IMA ADPCM 4-bit -> WAV PCM16)
//   +8  u32 originalSize
//   +12 u32 compressedSize
//   +16 u32 crc32(original)
//   +20 payload
//
// flags == 2 usa 16 bytes extra antes do payload (header de 36 bytes):
//   +20 u32 sampleRate
//   +24 u16 channels
//   +26 u16 bitsPerSample (16)
//   +28 u32 pcmDataBytes
//   +32 u32 dataOffset   (onde comeca o chunk 'data' no WAV original)
// O payload comeca com os 'dataOffset' bytes do WAV original (assim os chunks
// 'smpl' de loop point sobrevivem), seguidos de um bloco IMA por canal:
// [pred i16][stepIdx u8][pad u8] + nibbles. O decoder so troca o PCM.
//
// E o mesmo espirito do PRS da SEGA: decode rapido, sem alocacao, sem tabela
// grande e sem depender de heap do sistema. O encoder roda offline
// (tools/zeebo/re1pack.py); aqui so existe o decoder.
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RE1LZ_MAGIC_U32   0x5A4C3152u   /* 'R1LZ' em little-endian */
#define RE1LZ_HEADER_SIZE 20u
#define RE1LZ_ADPCM_HEADER_SIZE 36u
#define RE1LZ_FLAG_LZ4   1u
#define RE1LZ_FLAG_ADPCM 2u
#define RE1LZ_MAX_ORIGINAL (128u * 1024u * 1024u)

typedef struct RE1LZInfo {
    uint16_t version;
    uint16_t flags;
    uint32_t originalSize;
    uint32_t compressedSize;
    uint32_t crc32;
    /* somente quando flags == RE1LZ_FLAG_ADPCM */
    uint32_t sampleRate;
    uint16_t channels;
    uint16_t bitsPerSample;
    uint32_t pcmDataBytes;
    uint32_t dataOffset;
} RE1LZInfo;

/* 1 = cabecalho valido, 0 = nao e um arquivo R1LZ, -1 = corrompido. */
int RE1LZ_ParseHeader(const void* data, size_t size, RE1LZInfo* out);

/* Decodifica um bloco LZ4 cru. Devolve o numero de bytes escritos ou -1. */
int RE1LZ_DecompressBlock(const uint8_t* src, size_t srcSize,
                          uint8_t* dst, size_t dstCap);

/* CRC32 refletido (poly 0xEDB88320), compativel com zlib.crc32 do Python. */
uint32_t RE1LZ_Crc32(const void* data, size_t size);

/* Desempacota um arquivo inteiro.
 *   resultado 1 -> *outData e' um malloc() novo, *outSize e' o tamanho original;
 *                  o chamador deve free(*outData).
 *   resultado 0 -> nao era R1LZ; *outData aponta para os dados originais
 *                  (nao possuir), *outSize = size.
 *  -1 -> dados corrompidos.
 */
int RE1LZ_Unpack(const void* data, size_t size,
                 void** outData, size_t* outSize);

#ifdef __cplusplus
}
#endif
