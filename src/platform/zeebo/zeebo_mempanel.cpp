// zeebo_mempanel.cpp - Top-left on-screen memory & performance diagnostics panel for RE1 Zeebo.
#ifdef __ZEEBO__

#include "zeebo_mempanel.h"
#include "zeebo_brew.h"
#include "../platform.h"

#include "../../Globals.h"
#include "../../marni/MarniDX.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AEE_SIMULATOR
#include <windows.h>
#endif

namespace ZeeboMemPanel {

#ifdef AEE_SIMULATOR
static bool     s_enabled      = true;
#else
static bool     s_enabled      = false;
#endif

static uint32_t s_lastQueryMs  = 0;

// Cached BREW heap stats (refreshed at ~4 Hz to avoid walking the heap every frame)
static bool     s_hasHeapStats = false;
static uint32_t s_heapTotal    = 0;
static uint32_t s_heapFree     = 0;
static uint32_t s_heapUsed     = 0;
static uint32_t s_heapLargest  = 0;
static uint32_t s_heapMinFree  = 0xFFFFFFFFu;

#ifdef AEE_SIMULATOR
static uint32_t s_procPrivBytes = 0;
static uint32_t s_procWorkSet   = 0;
#endif

// Frame timing window (updated every ~15 frames for stable FPS readout)
static uint32_t s_winStartMs   = 0;
static uint32_t s_winFrames    = 0;
static uint32_t s_winDcSum     = 0;
static uint32_t s_winTriSum    = 0;
static uint32_t s_realFps      = 0;
static uint32_t s_frameMs      = 0;
static uint32_t s_avgDc        = 0;
static uint32_t s_avgTris      = 0;

// Compact 5x7 bitmap font atlas (ASCII 32..95, 16 cols x 4 rows of 6x8 cells -> 128x32)
static MarniHandle s_fontTex   = MARNI_NULL_HANDLE;
static bool        s_fontTried = false;

// 5x7 glyphs for ASCII 32..95 (5 columns per glyph, bit 0 = top row, bit 6 = bottom row)
static const uint8_t s_font5x7[64][5] = {
	{0x00,0x00,0x00,0x00,0x00}, // ' ' (32)
	{0x00,0x00,0x5F,0x00,0x00}, // '!' (33)
	{0x00,0x07,0x00,0x07,0x00}, // '"' (34)
	{0x14,0x7F,0x14,0x7F,0x14}, // '#' (35)
	{0x24,0x2A,0x7F,0x2A,0x12}, // '$' (36)
	{0x23,0x13,0x08,0x64,0x62}, // '%' (37)
	{0x36,0x49,0x55,0x22,0x50}, // '&' (38)
	{0x00,0x05,0x03,0x00,0x00}, // ''' (39)
	{0x00,0x1C,0x22,0x41,0x00}, // '(' (40)
	{0x00,0x41,0x22,0x1C,0x00}, // ')' (41)
	{0x14,0x08,0x3E,0x08,0x14}, // '*' (42)
	{0x08,0x08,0x3E,0x08,0x08}, // '+' (43)
	{0x00,0x50,0x30,0x00,0x00}, // ',' (44)
	{0x08,0x08,0x08,0x08,0x08}, // '-' (45)
	{0x00,0x60,0x60,0x00,0x00}, // '.' (46)
	{0x20,0x10,0x08,0x04,0x02}, // '/' (47)
	{0x3E,0x51,0x49,0x45,0x3E}, // '0' (48)
	{0x00,0x42,0x7F,0x40,0x00}, // '1' (49)
	{0x42,0x61,0x51,0x49,0x46}, // '2' (50)
	{0x21,0x41,0x45,0x4B,0x31}, // '3' (51)
	{0x18,0x14,0x12,0x7F,0x10}, // '4' (52)
	{0x27,0x45,0x45,0x45,0x39}, // '5' (53)
	{0x3C,0x4A,0x49,0x49,0x30}, // '6' (54)
	{0x01,0x71,0x09,0x05,0x03}, // '7' (55)
	{0x36,0x49,0x49,0x49,0x36}, // '8' (56)
	{0x06,0x49,0x49,0x29,0x1E}, // '9' (57)
	{0x00,0x36,0x36,0x00,0x00}, // ':' (58)
	{0x00,0x56,0x36,0x00,0x00}, // ';' (59)
	{0x08,0x14,0x22,0x41,0x00}, // '<' (60)
	{0x14,0x14,0x14,0x14,0x14}, // '=' (61)
	{0x00,0x41,0x22,0x14,0x08}, // '>' (62)
	{0x02,0x01,0x51,0x09,0x06}, // '?' (63)
	{0x32,0x49,0x79,0x41,0x3E}, // '@' (64)
	{0x7E,0x11,0x11,0x11,0x7E}, // 'A' (65)
	{0x7F,0x49,0x49,0x49,0x36}, // 'B' (66)
	{0x3E,0x41,0x41,0x41,0x22}, // 'C' (67)
	{0x7F,0x41,0x41,0x22,0x1C}, // 'D' (68)
	{0x7F,0x49,0x49,0x49,0x41}, // 'E' (69)
	{0x7F,0x09,0x09,0x09,0x01}, // 'F' (70)
	{0x3E,0x41,0x49,0x49,0x7A}, // 'G' (71)
	{0x7F,0x08,0x08,0x08,0x7F}, // 'H' (72)
	{0x00,0x41,0x7F,0x41,0x00}, // 'I' (73)
	{0x20,0x40,0x41,0x3F,0x01}, // 'J' (74)
	{0x7F,0x08,0x14,0x22,0x41}, // 'K' (75)
	{0x7F,0x40,0x40,0x40,0x40}, // 'L' (76)
	{0x7F,0x02,0x0C,0x02,0x7F}, // 'M' (77)
	{0x7F,0x04,0x08,0x10,0x7F}, // 'N' (78)
	{0x3E,0x41,0x41,0x41,0x3E}, // 'O' (79)
	{0x7F,0x09,0x09,0x09,0x06}, // 'P' (80)
	{0x3E,0x41,0x51,0x21,0x5E}, // 'Q' (81)
	{0x7F,0x09,0x19,0x29,0x46}, // 'R' (82)
	{0x46,0x49,0x49,0x49,0x31}, // 'S' (83)
	{0x01,0x01,0x7F,0x01,0x01}, // 'T' (84)
	{0x3F,0x40,0x40,0x40,0x3F}, // 'U' (85)
	{0x1F,0x20,0x40,0x20,0x1F}, // 'V' (86)
	{0x3F,0x40,0x38,0x40,0x3F}, // 'W' (87)
	{0x63,0x14,0x08,0x14,0x63}, // 'X' (88)
	{0x07,0x08,0x70,0x08,0x07}, // 'Y' (89)
	{0x61,0x51,0x49,0x45,0x43}, // 'Z' (90)
	{0x00,0x7F,0x41,0x41,0x00}, // '[' (91)
	{0x02,0x04,0x08,0x10,0x20}, // '\' (92)
	{0x00,0x41,0x41,0x7F,0x00}, // ']' (93)
	{0x04,0x02,0x01,0x02,0x04}, // '^' (94)
	{0x40,0x40,0x40,0x40,0x40}, // '_' (95)
};

void SetEnabled(bool enabled) { s_enabled = enabled; }
bool IsEnabled(void)          { return s_enabled; }
void Toggle(void)             { s_enabled = !s_enabled; }

static void EnsureFontTexture(MarniDX* dx)
{
	if (s_fontTex != MARNI_NULL_HANDLE || s_fontTried || !dx) return;
	s_fontTried = true;

	const int texW = 128;
	const int texH = 32;
	uint32_t* px = (uint32_t*)calloc((size_t)texW * (size_t)texH, sizeof(uint32_t));
	if (!px) return;

	for (int ch = 0; ch < 64; ++ch) {
		int col = ch & 15;
		int row = ch >> 4;
		int ox = col * 8;
		int oy = row * 8;
		for (int gx = 0; gx < 5; ++gx) {
			uint8_t bits = s_font5x7[ch][gx];
			for (int gy = 0; gy < 7; ++gy) {
				if (bits & (1u << gy)) {
					px[(oy + gy) * texW + (ox + gx)] = 0xFFFFFFFFu;
				}
			}
		}
	}

	s_fontTex = dx->CreateTexture(texW, texH, 32, px, NULL, NULL);
	free(px);
}

static void RefreshHeapStats(void)
{
	uint32_t now = plat_time_ms();
	if (s_lastQueryMs != 0 && (now - s_lastQueryMs) < 250u) {
		return;
	}
	s_lastQueryMs = now;

	if (g_Zeebo.pHelpers && g_Zeebo.pHelpers->pfnGetRAMFree) {
		uint32_t total = 0;
		uint32_t largest = 0;
		uint32_t freeBytes = g_Zeebo.pHelpers->pfnGetRAMFree(&total, &largest);
		if (total > 0 || freeBytes > 0) {
			s_hasHeapStats = true;
			s_heapTotal    = total;
			s_heapFree     = freeBytes;
			s_heapUsed     = (total >= freeBytes) ? (total - freeBytes) : 0;
			s_heapLargest  = largest;
			if (freeBytes < s_heapMinFree) {
				s_heapMinFree = freeBytes;
			}
		}
	}

#ifdef AEE_SIMULATOR
	typedef struct {
		DWORD  cb;
		DWORD  PageFaultCount;
		SIZE_T PeakWorkingSetSize;
		SIZE_T WorkingSetSize;
		SIZE_T QuotaPeakPagedPoolUsage;
		SIZE_T QuotaPagedPoolUsage;
		SIZE_T QuotaPeakNonPagedPoolUsage;
		SIZE_T QuotaNonPagedPoolUsage;
		SIZE_T PagefileUsage;
		SIZE_T PeakPagefileUsage;
		SIZE_T PrivateUsage;
	} SIM_PMC_EX;
	typedef BOOL (WINAPI *PFN_GetProcessMemoryInfo)(HANDLE, void*, DWORD);
	static PFN_GetProcessMemoryInfo s_pfnGPMI = NULL;
	static bool s_gpmiChecked = false;
	if (!s_gpmiChecked) {
		s_gpmiChecked = true;
		HMODULE hK32 = GetModuleHandleA("kernel32.dll");
		if (hK32) {
			s_pfnGPMI = (PFN_GetProcessMemoryInfo)GetProcAddress(hK32, "K32GetProcessMemoryInfo");
		}
		if (!s_pfnGPMI) {
			HMODULE hPsapi = LoadLibraryA("psapi.dll");
			if (hPsapi) {
				s_pfnGPMI = (PFN_GetProcessMemoryInfo)GetProcAddress(hPsapi, "GetProcessMemoryInfo");
			}
		}
	}
	if (s_pfnGPMI) {
		SIM_PMC_EX pmc;
		memset(&pmc, 0, sizeof(pmc));
		pmc.cb = sizeof(pmc);
		if (s_pfnGPMI(GetCurrentProcess(), &pmc, sizeof(pmc))) {
			s_procPrivBytes = (uint32_t)(pmc.PrivateUsage > 0xFFFFFFFFu ? 0xFFFFFFFFu : pmc.PrivateUsage);
			s_procWorkSet   = (uint32_t)(pmc.WorkingSetSize > 0xFFFFFFFFu ? 0xFFFFFFFFu : pmc.WorkingSetSize);
		}
	}
#endif
}

static void FormatBytes(uint32_t bytes, char* out, size_t outSize)
{
	if (bytes >= 1024u * 1024u) {
		float mb = (float)bytes / (1024.0f * 1024.0f);
		snprintf(out, outSize, "%.1fM", mb);
	} else {
		unsigned kb = (unsigned)(bytes / 1024u);
		snprintf(out, outSize, "%uK", kb);
	}
}

// Batch-emit text quads (2 tris per char, 8 floats/vert: x,y,u,v,r,g,b,a)
static float s_textVerts[1024 * 3 * 8];
static int   s_textTriCount = 0;

static void EmitChar(float x, float y, float scale, char c, float r, float g, float b, float a)
{
	if (s_textTriCount + 2 > 1024) return;
	if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
	if (c < 32 || c > 95) c = '?';
	int idx = (int)(c - 32);
	if (idx == 0) return; // space

	int col = idx & 15;
	int row = idx >> 4;
	const float invW = 1.0f / 128.0f;
	const float invH = 1.0f / 32.0f;
	float u0 = (float)(col * 8) * invW;
	float v0 = (float)(row * 8) * invH;
	float u1 = (float)(col * 8 + 5) * invW;
	float v1 = (float)(row * 8 + 7) * invH;
	float cw = 5.0f * scale;
	float ch = 7.0f * scale;

	float* dst = s_textVerts + s_textTriCount * 3 * 8;
	const float verts[6][4] = {
		{ x,      y,      u0, v0 },
		{ x + cw, y,      u1, v0 },
		{ x,      y + ch, u0, v1 },
		{ x + cw, y,      u1, v0 },
		{ x + cw, y + ch, u1, v1 },
		{ x,      y + ch, u0, v1 },
	};
	for (int i = 0; i < 6; ++i) {
		dst[i * 8 + 0] = verts[i][0];
		dst[i * 8 + 1] = verts[i][1];
		dst[i * 8 + 2] = verts[i][2];
		dst[i * 8 + 3] = verts[i][3];
		dst[i * 8 + 4] = r;
		dst[i * 8 + 5] = g;
		dst[i * 8 + 6] = b;
		dst[i * 8 + 7] = a;
	}
	s_textTriCount += 2;
}

static void EmitLine(float x, float y, float scale, const char* text, DWORD argb)
{
	float a = (float)((argb >> 24) & 0xFF) / 255.0f;
	float r = (float)((argb >> 16) & 0xFF) / 255.0f;
	float g = (float)((argb >>  8) & 0xFF) / 255.0f;
	float b = (float)((argb >>  0) & 0xFF) / 255.0f;
	float adv = 6.0f * scale;
	float cx = x;
	for (const char* p = text; *p; ++p, cx += adv) {
		EmitChar(cx + 1.0f, y + 1.0f, scale, *p, 0.0f, 0.0f, 0.0f, 0.85f);
		EmitChar(cx, y, scale, *p, r, g, b, a);
	}
}

void Draw(MarniDX* dx, uint32_t drawCalls, uint32_t tris, int activeTextures, int maxTextures)
{
	if (!s_enabled || !dx || !dx->IsReady()) {
		return;
	}

	// Update FPS/draw stats window
	uint32_t now = plat_time_ms();
	if (s_winStartMs == 0) {
		s_winStartMs = now;
	}
	s_winFrames++;
	s_winDcSum  += drawCalls;
	s_winTriSum += tris;
	uint32_t elapsed = now - s_winStartMs;
	if (elapsed >= 500u || s_winFrames >= 15u) {
		if (elapsed > 0u) {
			s_realFps = (s_winFrames * 1000u) / elapsed;
			s_frameMs = elapsed / s_winFrames;
		}
		s_avgDc   = s_winDcSum / s_winFrames;
		s_avgTris = s_winTriSum / s_winFrames;
		s_winStartMs = now;
		s_winFrames  = 0;
		s_winDcSum   = 0;
		s_winTriSum  = 0;
	}

	RefreshHeapStats();
	EnsureFontTexture(dx);

	const int padX   = 8;
	const int padY   = 8;
	const int panelW = 280;
	const int lineH  = 15;
	const int barH   = 7;
#ifdef AEE_SIMULATOR
	const int numLines = 7;
#else
	const int numLines = 6;
#endif
	const int panelH = 14 + barH + numLines * lineH + 8;

	uint32_t usedPct = 0;
	if (s_hasHeapStats && s_heapTotal > 0) {
		usedPct = (uint32_t)(((uint64_t)s_heapUsed * 100u) / s_heapTotal);
		if (usedPct > 100u) usedPct = 100u;
	}

	DWORD statusCol = 0xFF5AEB6Eu; // Green
	if (s_hasHeapStats) {
		if (usedPct >= 85u || s_heapFree < 2u * 1024u * 1024u) {
			statusCol = 0xFFFF4B4Bu; // Red
		} else if (usedPct >= 70u || s_heapFree < 4u * 1024u * 1024u) {
			statusCol = 0xFFFFD23Cu; // Yellow
		}
	}

	// Translucent dark panel background (no left accent border)
	dx->DrawRect(padX, padY, panelW, panelH, 0xB4080A0Eu);

	// Heap usage progress bar
	const int bx = padX + 8;
	const int bw = panelW - 16;
	const int by = padY + 7;
	dx->DrawRect(bx, by, bw, barH, 0xDC2D323Cu);
	if (s_hasHeapStats && s_heapTotal > 0) {
		int fillW = (int)(((uint64_t)bw * s_heapUsed) / s_heapTotal);
		if (fillW < 0) fillW = 0;
		if (fillW > bw) fillW = bw;
		if (fillW > 0) {
			dx->DrawRect(bx, by, fillW, barH, statusCol);
		}
	}

	if (s_fontTex == MARNI_NULL_HANDLE) {
		return;
	}

	const DWORD colWhite = 0xFFEBF0F5u;
	const DWORD colCyan  = 0xFF78D7FFu;
	const DWORD colDim   = 0xFFB9C3CDu;

	const float tx    = (float)(padX + 8);
	float       ty    = (float)(by + barH + 5);
	const float scale = 1.6f;
	char line[96];

	s_textTriCount = 0;

	// Line 1: Heap Used / Total (%)
	if (s_hasHeapStats) {
		char usedBuf[16], totBuf[16];
		FormatBytes(s_heapUsed, usedBuf, sizeof(usedBuf));
		FormatBytes(s_heapTotal, totBuf, sizeof(totBuf));
		snprintf(line, sizeof(line), "HEAP %s/%s (%u%%)", usedBuf, totBuf, (unsigned)usedPct);
		EmitLine(tx, ty, scale, line, statusCol);
	} else {
		EmitLine(tx, ty, scale, "HEAP N/A", colDim);
	}
	ty += (float)lineH;

	// Line 2: Free RAM + Largest Contiguous Block
	if (s_hasHeapStats) {
		char freeBuf[16], maxBuf[16];
		FormatBytes(s_heapFree, freeBuf, sizeof(freeBuf));
		FormatBytes(s_heapLargest, maxBuf, sizeof(maxBuf));
		snprintf(line, sizeof(line), "FREE %s  MAX %s", freeBuf, maxBuf);
		EmitLine(tx, ty, scale, line, colWhite);
	} else {
		EmitLine(tx, ty, scale, "FREE N/A  MAX N/A", colDim);
	}
	ty += (float)lineH;

	// Line 3: Lowest Free RAM seen
	if (s_hasHeapStats && s_heapMinFree != 0xFFFFFFFFu) {
		char lowBuf[16];
		FormatBytes(s_heapMinFree, lowBuf, sizeof(lowBuf));
		snprintf(line, sizeof(line), "LOW  %s", lowBuf);
		EmitLine(tx, ty, scale, line, colDim);
	} else {
		EmitLine(tx, ty, scale, "LOW  N/A", colDim);
	}
	ty += (float)lineH;

	// Line 4: RE1 Stage / Room / Active Enemies
	snprintf(line, sizeof(line), "STG %d  RM %02X  ENM %d",
	         (int)g_stageId, (unsigned)(g_roomId & 0xFFu), (int)g_enemy_count);
	EmitLine(tx, ty, scale, line, colCyan);
	ty += (float)lineH;

	// Line 5: FPS / Frame ms / Draw Calls / Triangles
	{
		float kTris = (float)s_avgTris / 1000.0f;
		snprintf(line, sizeof(line), "FPS %u (%ums) DC %u %.1fK",
		         (unsigned)s_realFps, (unsigned)s_frameMs, (unsigned)s_avgDc, kTris);
		EmitLine(tx, ty, scale, line, colWhite);
	}
	ty += (float)lineH;

	// Line 6: Active MarniDX Textures
	snprintf(line, sizeof(line), "TEX %d/%d", activeTextures, maxTextures);
	EmitLine(tx, ty, scale, line, colDim);

#ifdef AEE_SIMULATOR
	ty += (float)lineH;
	{
		char privBuf[16], wsBuf[16];
		FormatBytes(s_procPrivBytes, privBuf, sizeof(privBuf));
		FormatBytes(s_procWorkSet, wsBuf, sizeof(wsBuf));
		snprintf(line, sizeof(line), "SIM  %s  WS %s", privBuf, wsBuf);
		EmitLine(tx, ty, scale, line, colDim);
	}
#endif

	if (s_textTriCount > 0) {
		dx->DrawTriangles(s_textVerts, s_textTriCount, s_fontTex,
		                  MARNI_SAMPLER_POINT, MARNI_BLEND_ALPHA);
	}
}

} // namespace ZeeboMemPanel

#endif // __ZEEBO__
