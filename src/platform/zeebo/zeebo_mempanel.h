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

} // namespace ZeeboMemPanel

#endif // __ZEEBO__
