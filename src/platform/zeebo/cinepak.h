// cinepak.h - Lightweight Cinepak (CVID) software video decoder for Zeebo.
#pragma once

#include <stdint.h>

typedef struct CinepakDecoder CinepakDecoder;

CinepakDecoder* Cinepak_Create(int width, int height);
void            Cinepak_Destroy(CinepakDecoder* dec);

// Decode one Cinepak frame into the internal RGBA8888 buffer.
// Returns 1 on success, 0 on invalid/truncated packet.
int             Cinepak_DecodeFrame(CinepakDecoder* dec, const uint8_t* data, uint32_t size);

// Pointer to the decoded RGBA8888 framebuffer (width * height * 4 bytes).
const uint8_t*  Cinepak_GetRGBA(const CinepakDecoder* dec);
