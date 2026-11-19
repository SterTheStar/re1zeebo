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
