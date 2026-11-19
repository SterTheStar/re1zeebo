// avi_demux.h - Lightweight RIFF/AVI container reader for Zeebo FMVs.
//
// Reads Resident Evil 1 PC Cinepak AVI files from memory/IFileMgr without
// external dependencies like libavformat.
#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct AviDemux AviDemux;

typedef struct AviVideoInfo {
    int      width;
    int      height;
    double   fps;
    uint32_t totalFrames;
    uint32_t fourcc;
    uint16_t audioFormat;
    uint16_t audioChannels;
    uint32_t audioSampleRate;
    uint32_t audioAvgBytesPerSec;
    uint16_t audioBlockAlign;
    uint16_t audioBitsPerSample;
} AviVideoInfo;

AviDemux* AviDemux_Open(const char* path);
void      AviDemux_Close(AviDemux* avi);

const AviVideoInfo* AviDemux_GetInfo(const AviDemux* avi);
int       AviDemux_GetDurationMs(const AviDemux* avi);

// Reset stream cursor to the start of the movi list.
void      AviDemux_Rewind(AviDemux* avi);

// Read the next video chunk ('00dc' / '00db') into *outData (pointer into
// internal buffer valid until next call). Returns 1 on success, 0 on EOF.
int       AviDemux_NextVideoFrame(AviDemux* avi, const uint8_t** outData, uint32_t* outSize);

// Extract interleaved PCM audio ('01wb') starting at fromMs into a newly
// malloc'd RIFF/WAVE buffer. Caller must free(*outWav). Returns 1 on success.
int       AviDemux_ExtractWav(AviDemux* avi, int fromMs, uint8_t** outWav, uint32_t* outSize);
