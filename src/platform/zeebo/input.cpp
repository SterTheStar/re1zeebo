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

// Standard BREW / Tectoy IHID Joystick Button & Axis UIDs (AEEHIDDevice_Joystick.h)
#define ZEEBO_UID_DPAD_UP         0x0106c3feu
#define ZEEBO_UID_DPAD_LEFT       0x0106c3ffu
#define ZEEBO_UID_DPAD_DOWN       0x0106c400u
#define ZEEBO_UID_DPAD_RIGHT      0x0106c401u
#define ZEEBO_UID_START           0x0106c402u
#define ZEEBO_UID_BACK            0x0106c403u
#define ZEEBO_UID_LTHUMB          0x0106c404u
#define ZEEBO_UID_RTHUMB          0x0106c405u
#define ZEEBO_UID_ZL              0x0106c406u
#define ZEEBO_UID_L2              0x0106c407u
#define ZEEBO_UID_ZR              0x0106c408u
#define ZEEBO_UID_R2              0x0106c409u
#define ZEEBO_UID_BTN_1           0x0106c40au
#define ZEEBO_UID_BTN_2           0x0106c40bu
#define ZEEBO_UID_BTN_3           0x0106c40cu
#define ZEEBO_UID_BTN_4           0x0106c40du

#define ZEEBO_UID_AXIS_RX         0x0106c4ceu
#define ZEEBO_UID_AXIS_RY         0x0106c4cfu
#define ZEEBO_UID_AXIS_LX         0x0106c4d0u
#define ZEEBO_UID_AXIS_LY         0x0106c4d1u

int32_t s_axisMin[25];
int32_t s_axisMax[25];
int32_t s_axisUid[25];
int     s_idxLX = 1;
int     s_idxLY = 2;
boolean s_axesQueried = FALSE;

void QueryZeeboPadAxes(IHIDDevice* dev)
{
    for (int i = 0; i < 25; ++i) {
        s_axisMin[i] = 0;
        s_axisMax[i] = 0;
        s_axisUid[i] = 0;
    }
    s_idxLX = 1;
    s_idxLY = 2;
    s_axesQueried = FALSE;
    if (!dev || !dev->pvt) return;

    boolean hasMin = (dev->pvt->GetMinPositionInfo && dev->pvt->GetMinPositionInfo(dev, s_axisMin) == AEE_SUCCESS);
    boolean hasMax = (dev->pvt->GetMaxPositionInfo && dev->pvt->GetMaxPositionInfo(dev, s_axisMax) == AEE_SUCCESS);
    boolean hasUid = (dev->pvt->GetAxesInfo && dev->pvt->GetAxesInfo(dev, s_axisUid) == AEE_SUCCESS);

    if (hasUid) {
        for (int i = 1; i < 25; ++i) {
            uint32_t u = (uint32_t)s_axisUid[i];
            if (u == ZEEBO_UID_AXIS_LX)      s_idxLX = i;
            else if (u == ZEEBO_UID_AXIS_LY) s_idxLY = i;
        }
    }
    s_axesQueried = (hasMin && hasMax) ? TRUE : FALSE;
}

int NormalizeHidAxisTo32767(int slot, const int32_t* posInfo)
{
    if (slot < 1 || slot >= 25) return 0;
    int32_t raw = posInfo[slot];

    if (s_axesQueried) {
        int32_t mn = s_axisMin[slot];
        int32_t mx = s_axisMax[slot];
        // In BREW 4.0.2 Simulator IHID.dll, unpresent/unmapped axes report min==max (0..0).
        // Return 0 (centered) instead of computing (0 - 128) * 256 = -32768 (constant UP/LEFT drift!).
        if (mx <= mn) return 0;
        int32_t center = (mn + mx) / 2;
        int32_t half   = (mx - mn) / 2;
        if (half <= 0) return 0;
        int32_t norm = ((raw - center) * 32767) / half;
        if (norm < -32768) norm = -32768;
        else if (norm > 32767) norm = 32767;
        return (int)norm;
    }

#ifdef AEE_SIMULATOR
    if (raw == 0) return 0;
#endif
    int v = (raw - 128) * 256;
    if (v < -32768) v = -32768;
    else if (v > 32767) v = 32767;
    return v;
}

int MapHidButtonToZeeboIndex(const AEEHIDButtonInfo& btn)
{
    switch (btn.dwUID) {
    case ZEEBO_UID_BTN_2:      return 0;  // Z-Pad Button 1 (South / A)
    case ZEEBO_UID_BTN_3:      return 1;  // Z-Pad Button 2 (East / B)
    case ZEEBO_UID_BTN_1:      return 2;  // Z-Pad Button 4 (West / X)
    case ZEEBO_UID_BTN_4:      return 3;  // Z-Pad Button 3 (North / Y)
    case ZEEBO_UID_ZR:         return 4;  // ZR (RB)
    case ZEEBO_UID_L2:         return 5;  // L2
    case ZEEBO_UID_ZL:         return 6;  // ZL (LB)
    case ZEEBO_UID_R2:         return 7;  // R2
    case ZEEBO_UID_RTHUMB:     return 8;  // R3
    case ZEEBO_UID_BACK:       return 9;  // Back
    case ZEEBO_UID_LTHUMB:     return 10; // L3
    case ZEEBO_UID_START:      return 11; // HOME / Start
    case ZEEBO_UID_DPAD_UP:    return 12; // D-Pad UP
    case ZEEBO_UID_DPAD_LEFT:  return 13; // D-Pad LEFT
    case ZEEBO_UID_DPAD_DOWN:  return 14; // D-Pad DOWN
    case ZEEBO_UID_DPAD_RIGHT: return 15; // D-Pad RIGHT
    default:
        break;
    }
    return (btn.dwID < 32) ? (int)btn.dwID : -1;
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
            QueryZeeboPadAxes(dev);
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
        int idx = MapHidButtonToZeeboIndex(btn);
        if (idx >= 0 && idx < 32) {
            if (btn.dwState) {
                s_hidButtonState |= (1u << idx);
            } else {
                s_hidButtonState &= ~(1u << idx);
            }
        }
    }

    DWORD mask = 0;
    for (uint32_t i = 0; i < 16; ++i) {
        if (s_hidButtonState & (1u << i)) {
            mask |= MapZeeboButtonIndexToMarniBit(i);
        }
    }

    // Read analog stick axes normalized via GetMinPositionInfo/GetMaxPositionInfo/GetAxesInfo
    int32_t posInfo[25];
    for (int i = 0; i < 25; ++i) posInfo[i] = 128;
    if (s_hidDevice->pvt->GetPositionState(s_hidDevice, posInfo) == AEE_SUCCESS) {
        int lx = NormalizeHidAxisTo32767(s_idxLX, posInfo);
        int ly = NormalizeHidAxisTo32767(s_idxLY, posInfo);
#ifdef AEE_SIMULATOR
        // BREW 4.0.2 Simulator IHID.dll inverts Y axis (INVERT:Y in hid_devices.cfg -> UP=+32767, DOWN=-32768),
        // whereas Zeebo expects UP < 0 and DOWN > 0.
        ly = -ly;
#endif
        if (ly < -s_deadzone) mask |= 0x00000001u; // UP
        if (ly >  s_deadzone) mask |= 0x00000002u; // DOWN
        if (lx < -s_deadzone) mask |= 0x00000004u; // LEFT
        if (lx >  s_deadzone) mask |= 0x00000008u; // RIGHT
    }

    return mask;
}

} // namespace

// ---------------------------------------------------------------------------
// platform.h keyboard API (no EVT_KEY fallback; Zeebo uses IHID exclusively)
// ---------------------------------------------------------------------------
extern "C" boolean Zeebo_OnKeyEvent(AEEEvent evt, uint16_t keyCode, uint32_t dwParam)
{
    (void)evt;
    (void)keyCode;
    (void)dwParam;
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
DWORD Poll() { return s_xinputEnabled ? PollZeeboHidMask() : 0u; }

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
