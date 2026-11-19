// stubs.cpp - Zeebo placeholder for Win32 ShowMessageBox.
#include "../platform.h"
#include "../../Globals.h"

int ShowMessageBox(HWND hWnd, LPCSTR lpMsg, LPCSTR lpCaption, UINT uType)
{
    (void)hWnd;
    (void)uType;
    plat_debug_output("[MSG] ");
    if (lpCaption) {
        plat_debug_output(lpCaption);
        plat_debug_output(": ");
    }
    if (lpMsg) {
        plat_debug_output(lpMsg);
    }
    plat_debug_output("\n");
    return 1; // IDOK
}
