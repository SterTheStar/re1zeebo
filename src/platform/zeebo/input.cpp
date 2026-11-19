// input.cpp - Complete Zeebo Z-Pad / Boomerang (IHID) + BREW EVT_KEY backend.
//
// Combines:
//   1. BREW IHID / IHIDDevice polling for Zeebo's 16-button Z-Pad + analog stick
//      (matching zeebx-emu's exact BUTTON_UIDS indices and AXIS_CENTER=128).
//   2. BREW EVT_KEY / EVT_KEY_PRESS / EVT_KEY_RELEASE fallback for console
//      navigation keys and keyboard testing.
//   3. Full MarniXInput namespace + UpdateAllInputStates(MasterInputState*)
//      populating joysticks[0] with exact MARNI_XI_BTN_* bit assignments.
#include "../platform.h"
#include "zeebo_brew.h"

#include "../../Globals.h"
#include "../../marni/MarniInput.h"
#include "../../marni/MarniXInput.h"

#include <string.h>

namespace {

uint8_t s_keyDown[256];
uint8_t s_keyPressedEdge[256];

// Zeebo IHID gamepad state
IHIDDevice* s_hidDevice      = NULL;
DWORD       s_nextRescanMs   = 0;
uint32_t    s_hidButtonState = 0; // bit i = button index i from GetNextButtonEvent

bool s_xinputEnabled = true;
int  s_deadzone      = MARNI_XI_DEFAULT_DEADZONE;

// Map Z-Pad button index (zeebx-emu BUTTON_UIDS order) to Marni mask bit.
DWORD MapZeeboButtonIndexToMarniBit(uint32_t idx)
{
    switch (idx) {
    case 0:  return MARNI_XI_BTN_A;         // b1
    case 1:  return MARNI_XI_BTN_B;         // b2
    case 2:  return MARNI_XI_BTN_X;         // b3
    case 3:  return MARNI_XI_BTN_Y;         // b4
    case 4:  return MARNI_XI_BTN_RB;        // zr
    case 5:  return MARNI_XI_BTN_LTRIGGER;  // l2
    case 6:  return MARNI_XI_BTN_LB;        // zl
    case 7:  return MARNI_XI_BTN_RTRIGGER;  // r2
    case 8:  return MARNI_XI_BTN_RTHUMB;    // rthumb
    case 9:  return MARNI_XI_BTN_BACK;      // back / HOME
    case 10: return MARNI_XI_BTN_LTHUMB;    // lthumb
    case 11: return MARNI_XI_BTN_START;     // start
    case 12: return 0x00000001u;            // D-Pad UP
    case 13: return 0x00000004u;            // D-Pad LEFT
    case 14: return 0x00000002u;            // D-Pad DOWN
    case 15: return 0x00000008u;            // D-Pad RIGHT
    default: return 0u;
    }
}

void TryOpenZeeboPad(void)
{
    if (s_hidDevice != NULL || !g_Zeebo.pHID || !g_Zeebo.pHID->pvt) {
        return;
    }

    DWORD now = plat_time_ms();
    if (now < s_nextRescanMs) {
        return;
    }
    s_nextRescanMs = now + 1000;

    int handles[4] = { 0 };
    int needed = 0;
    int rc = g_Zeebo.pHID->pvt->GetConnectedDevices(
        g_Zeebo.pHID, (int)AEEHID_UID_JOYSTICK_DEVICE, handles, 4, &needed);
    if (rc == AEE_SUCCESS && needed > 0 && handles[0] != 0) {
        IHIDDevice* dev = NULL;
        if (g_Zeebo.pHID->pvt->CreateDevice(g_Zeebo.pHID, handles[0], &dev) == AEE_SUCCESS && dev) {
            s_hidDevice = dev;
            s_hidButtonState = 0;
        }
    }
}

DWORD PollZeeboHidMask(void)
{
    TryOpenZeeboPad();
    if (!s_hidDevice || !s_hidDevice->pvt) {
        return 0;
    }

    // Drain queued button press/release events from IHIDDevice
    AEEHIDButtonInfo btn;
    uint32_t ts = 0;
    boolean dropped = FALSE;
    while (s_hidDevice->pvt->GetNextButtonEvent(s_hidDevice, &btn, &ts, &dropped) == AEE_SUCCESS) {
        if (btn.dwID < 32) {
            if (btn.dwState) {
                s_hidButtonState |= (1u << btn.dwID);
            } else {
                s_hidButtonState &= ~(1u << btn.dwID);
            }
        }
    }

    DWORD mask = 0;
    for (uint32_t i = 0; i < 16; ++i) {
        if (s_hidButtonState & (1u << i)) {
            mask |= MapZeeboButtonIndexToMarniBit(i);
        }
    }

    // Read analog stick axes (AEEHIDPositionInfo: 25 words; X=slot 1, Y=slot 2)
    int32_t posInfo[25];
    for (int i = 0; i < 25; ++i) posInfo[i] = 128;
    if (s_hidDevice->pvt->GetPositionState(s_hidDevice, posInfo) == AEE_SUCCESS) {
        // Scale 0..255 (center 128) -> -32767..32767 for Marni deadzone comparison
        int lx = (posInfo[1] - 128) * 256;
        int ly = (posInfo[2] - 128) * 256;
        if (ly < -s_deadzone) mask |= 0x00000001u; // UP
        if (ly >  s_deadzone) mask |= 0x00000002u; // DOWN
        if (lx < -s_deadzone) mask |= 0x00000004u; // LEFT
        if (lx >  s_deadzone) mask |= 0x00000008u; // RIGHT
    }

    return mask;
}

int MapBrewKeyToVK(uint16_t brewKey)
{
    switch (brewKey) {
    case AVK_UP:     return 0x26; // VK_UP
    case AVK_DOWN:   return 0x28; // VK_DOWN
    case AVK_LEFT:   return 0x25; // VK_LEFT
    case AVK_RIGHT:  return 0x27; // VK_RIGHT
    case AVK_SELECT: return 0x0D; // VK_RETURN
    case AVK_CLR:    return 0x1B; // VK_ESCAPE
    case AVK_SOFT1:  return 0x20; // VK_SPACE
    case AVK_SOFT2:  return 0x09; // VK_TAB
    case AVK_1:      return '1';
    case AVK_2:      return '2';
    case AVK_3:      return '3';
    case AVK_4:      return '4';
    default:         return 0;
    }
}

DWORD BuildFallbackKeyMask(void)
{
    DWORD mask = 0;
    if (s_keyDown[0x26]) mask |= 0x00000001u; // UP
    if (s_keyDown[0x28]) mask |= 0x00000002u; // DOWN
    if (s_keyDown[0x25]) mask |= 0x00000004u; // LEFT
    if (s_keyDown[0x27]) mask |= 0x00000008u; // RIGHT
    if (s_keyDown[0x0D]) mask |= MARNI_XI_BTN_A;
    if (s_keyDown[0x1B]) mask |= MARNI_XI_BTN_B;
    if (s_keyDown[0x20]) mask |= MARNI_XI_BTN_X;
    if (s_keyDown[0x09]) mask |= MARNI_XI_BTN_START;
    return mask;
}

} // namespace

// ---------------------------------------------------------------------------
// BREW EVT_KEY handler + platform.h keyboard API
// ---------------------------------------------------------------------------
extern "C" boolean Zeebo_OnKeyEvent(AEEEvent evt, uint16_t keyCode, uint32_t dwParam)
{
    (void)dwParam;
    int vk = MapBrewKeyToVK(keyCode);
    if (vk <= 0 || vk >= 256) {
        return FALSE;
    }

    if (evt == EVT_KEY_PRESS || evt == EVT_KEY) {
        plat_key_event(vk, TRUE);
        return TRUE;
    }
    if (evt == EVT_KEY_RELEASE) {
        plat_key_event(vk, FALSE);
        return TRUE;
    }
    return FALSE;
}

int plat_key_state(int vk)
{
    if (vk < 0 || vk >= 256) return 0;
    int result = 0;
    if (s_keyDown[vk]) result |= 0x8000;
    if (s_keyPressedEdge[vk]) {
        result |= 0x0001;
        s_keyPressedEdge[vk] = 0;
    }
    return result;
}

void plat_key_event(int keycode, BOOL down)
{
    if (keycode < 0 || keycode >= 256) return;
    if (down) {
        if (!s_keyDown[keycode]) s_keyPressedEdge[keycode] = 1;
        s_keyDown[keycode] = 1;
    } else {
        s_keyDown[keycode] = 0;
    }
}

void plat_key_flush(void)
{
    memset(s_keyPressedEdge, 0, sizeof(s_keyPressedEdge));
}

// ---------------------------------------------------------------------------
// MarniXInput & UpdateAllInputStates
// ---------------------------------------------------------------------------
namespace MarniXInput {

void SetEnabled(bool enabled) { s_xinputEnabled = enabled; }
bool IsEnabled() { return s_xinputEnabled; }
void SetDeadzone(int deadzone) { s_deadzone = (deadzone > 0) ? deadzone : MARNI_XI_DEFAULT_DEADZONE; }
void Init() { TryOpenZeeboPad(); }
bool IsConnected() { return s_hidDevice != NULL; }
DWORD Poll() { return s_xinputEnabled ? (PollZeeboHidMask() | BuildFallbackKeyMask()) : 0u; }

} // namespace MarniXInput

void CMarniDirectInput::InitJoysticks(MasterInputState* state)
{
    MarniXInput::Init();
    if (state) {
        state->joystickCount = (s_hidDevice != NULL) ? 1 : 0;
        state->joysticks[0].enabled = 1;
    }
}

void CMarniDirectInput::UpdateAllInputStates(MasterInputState* state)
{
    if (!state) return;
    state->frameFlag = 1;

    DWORD mask = MarniXInput::Poll();
    JoystickEntry* joy = &state->joysticks[0];
    joy->enabled   = 1;
    joy->prevPress = joy->currPress;
    joy->currPress = mask;
    joy->newPress  = (~joy->prevPress) & joy->currPress;
    state->joystickCount = (s_hidDevice != NULL) ? 1 : 0;
}

bool MarniPadIsConnected()
{
    return MarniXInput::IsConnected();
}
