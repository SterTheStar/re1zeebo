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

// Satisfy weak undefined newlib/libgcc references so elf2mod never sees
// SHN_UNDEF relocations outside PT_LOAD segments.
extern "C" {
void* __gnu_Unwind_Find_exidx(void* pc, int* pcount)
{
    (void)pc;
    if (pcount) *pcount = 0;
    return 0;
}

void __libc_fini(void)
{
}
}
