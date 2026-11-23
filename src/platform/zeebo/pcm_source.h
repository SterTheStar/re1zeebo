// pcm_source.h - ISource de PCM cru alimentado por um container R1LZ/ADPCM
// dentro do RE1DATA.PAK. Nenhum WAV inteiro e' alocado: a janela de nibbles
// e' pequena (2 x 4 KB) e o PCM e' produzido sob demanda para o IMedia.
#pragma once

#include "zeebo_brew.h"
#include "re1_archive.h"
#include "re1_lz4.h"

#define RE1_PCM_WINDOW 4096u

typedef struct RE1PcmSource {
    ISource             base;          // deve ser o primeiro campo
    uint32_t            refCount;
    const RE1ArcEntry*  entry;
    RE1LZInfo           info;
    uint32_t            samplesPerCh;
    uint32_t            blockBytes;
    uint32_t            posSamples;    // frames ja' entregues
    int                 pred[2];
    int                 idx[2];
    uint8_t             win[2][RE1_PCM_WINDOW];
    uint32_t            winBase[2];    // indice absoluto do 1o byte da janela
} RE1PcmSource;

int    RE1PcmSource_Init(RE1PcmSource* src, const RE1ArcEntry* entry);
void   RE1PcmSource_Reset(RE1PcmSource* src);
int32_t RE1PcmSource_ReadPcm(RE1PcmSource* src, void* dst, int32_t bytes);
ISource* RE1PcmSource_AsISource(RE1PcmSource* src);

// Configura a spec de PCM cru usada pelo AEECLSID_MEDIAPCM.
void RE1Pcm_FillWaveSpec(AEEMediaWaveSpec* spec, const RE1LZInfo* info);
void RE1Pcm_FillMediaData(AEEMediaDataEx* md, ISource* source,
                          AEEMediaWaveSpec* spec, uint32_t bufferSize);
