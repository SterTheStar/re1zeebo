// zeebo_mempanel.h - Top-left on-screen memory & performance diagnostics panel for RE1 Zeebo.
#pragma once

#ifdef __ZEEBO__

#include <stdint.h>

class MarniDX;

namespace ZeeboMemPanel {

void SetEnabled(bool enabled);
bool IsEnabled(void);
void Toggle(void);

// Draw the top-left overlay panel (call right before eglSwapBuffers in MarniDX::Present).
void Draw(MarniDX* dx, uint32_t drawCalls, uint32_t tris, int activeTextures, int maxTextures);

// Per-frame timing accumulators (ms) for CPU hotspot diagnosis.
extern uint32_t g_profTickMs;
extern uint32_t g_profEntMs;
extern uint32_t g_profTmdMs;
extern uint32_t g_profSwapMs;

} // namespace ZeeboMemPanel

#endif // __ZEEBO__
