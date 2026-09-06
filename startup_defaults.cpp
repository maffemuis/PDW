// First-run startup defaults and bootstrap files for the modern portable build.
// Existing PDW.ini settings always win; this adapter only changes a clean first start.

#ifndef STRICT
#define STRICT 1
#endif

#include <windows.h>
#include <string.h>

#include "headers\pdw.h"
#include "headers\initapp.h"

BOOL LegacyGetPrivateProfileSettings(LPCTSTR lpszAppTitle, LPCTSTR lpszIniPathName, PPROFILE pProfile);

static bool PathMissing(LPCTSTR path)
{
    return GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES;
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
        // Clean-install defaults requested for the modern standalone build:
        // serial input enabled, COM2 selected, RS232 FLEX-1600 mode selected,
        // and sound-card capture disabled so there is one unambiguous input path.
        // Existing installations retain whatever is already stored in PDW.ini.
        pProfile->comPortEnabled = 1;
        pProfile->comPort = 2;
        pProfile->comPortRS232 = 2; // 1=Pocsag, 2=Flex-1600, 3=Mobitex
        pProfile->audioEnabled = 0;

        pProfile->monitor_paging = TRUE;
        pProfile->decodeflex = 1;
        pProfile->flex_1600 = 1;
    }

    return TRUE;
}
