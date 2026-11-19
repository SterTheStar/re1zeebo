// config.cpp - Settings persistence & single-instance guard on Zeebo.
#include "../platform.h"
#include "../../Globals.h"
#include "../../system/ConfigFile.h"

void CleanupVideoConfigAndSaveAllSettings(void)
{
    if (g_bHasFinalizedSettings) {
        return;
    }
    g_bHasFinalizedSettings = TRUE;
    ConfigFile_Save();
}

BOOL plat_single_instance_check(void)
{
    // BREW only runs one foreground applet instance at a time.
    return TRUE;
}
