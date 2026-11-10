// First-run startup defaults and bootstrap files for the modern portable build.
// Existing PDW.ini settings always win; this adapter only changes a clean first start.

#ifndef STRICT
#define STRICT 1
#endif

#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>

#include "headers\pdw.h"
#include "headers\initapp.h"
#include "core/startup_input_selection.h"

BOOL LegacyGetPrivateProfileSettings(LPCTSTR lpszAppTitle, LPCTSTR lpszIniPathName, PPROFILE pProfile);

static bool PathMissing(LPCTSTR path)
{
    return GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES;
}

static int DetectFirstPresentComPort()
{
    char deviceName[16];
    char target[1024];

    // QueryDosDevice reports the COM device names Windows currently exposes.
    // Scan the full legacy-supported numeric range instead of assuming COM1/COM2.
    for (int port = 1; port <= 256; ++port)
    {
        sprintf(deviceName, "COM%d", port);
        if (QueryDosDeviceA(deviceName, target, sizeof(target)) != 0)
            return port;
    }

    return 0;
}

static void EnsureFirstRunFiles()
{
    // filters.ini is optional user state. On a clean install, bootstrap a valid
    // empty file so the legacy loader does not present a misleading error dialog.
    HANDLE filters = CreateFile(
        szFilterPathName,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (filters != INVALID_HANDLE_VALUE)
    {
        static const char kEmptyFilters[] = "[Filter]\r\nFilterCount=0\r\n";
        DWORD written = 0;
        WriteFile(filters, kEmptyFilters, (DWORD)strlen(kEmptyFilters), &written, NULL);
        CloseHandle(filters);
    }

    // The log directory is normal runtime state as well; create it quietly on
    // first use instead of waiting for an options dialog to do so.
    CreateDirectory(szLogPathName, NULL);
}

BOOL GetPrivateProfileSettings(LPCTSTR lpszAppTitle, LPCTSTR lpszIniPathName, PPROFILE pProfile)
{
    const bool firstRun = PathMissing(lpszIniPathName);

    EnsureFirstRunFiles();

    const BOOL loaded = LegacyGetPrivateProfileSettings(lpszAppTitle, lpszIniPathName, pProfile);
    if (!loaded)
        return FALSE;

    if (firstRun)
    {
        const StartupInputSelection input = ChooseStartupInput(
            waveInGetNumDevs(), DetectFirstPresentComPort());

        // Use soundcard capture on first run when Windows exposes it.
        // Otherwise use the first COM port if present; never both.
        // Existing PDW.ini settings are not changed.
        pProfile->comPort = input.comPort;
        pProfile->comPortEnabled = input.comPortEnabled;
        pProfile->comPortRS232 = 2; // 1=Pocsag, 2=Flex-1600, 3=Mobitex
        pProfile->audioDevice = input.audioDevice;
        pProfile->audioEnabled = input.audioEnabled;

        pProfile->monitor_paging = TRUE;
        pProfile->decodeflex = 1;
        pProfile->flex_1600 = 1;
    }

    return TRUE;
}
