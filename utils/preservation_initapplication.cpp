#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "..\Headers\pdw.h"
#include "..\Headers\initapp.h"
#include "..\Headers\sound_in.h"

BOOL NEAR LegacyInitApplication(HINSTANCE hInstance);

namespace
{
bool PreservationOneShotReplayRequested()
{
    const char *recording = getenv("PDW_PRESERVATION_REPLAY_WAV");
    const char *one_shot = getenv("PDW_PRESERVATION_REPLAY_EXIT");

    return recording && recording[0]
        && one_shot && strcmp(one_shot, "1") == 0;
}
}

BOOL NEAR InitApplication(HINSTANCE hInstance)
{
    if (!PreservationOneShotReplayRequested())
    {
        return LegacyInitApplication(hInstance);
    }

    // WinMain has already initialized PDW's deterministic built-in Profile
    // defaults before calling InitApplication(). In one-shot preservation mode
    // replay immediately, before INI loading, window-class registration or any
    // other GUI startup work can block a headless CI runner.

    // Legacy GUI startup normally initializes the POCSAG BCH/ECC lookup tables
    // later during window creation. Headless one-shot replay bypasses that path,
    // so initialize the same decoder state explicitly here.
    setupecc();

    // Start_Capturing() terminates with exit 0/2 in one-shot replay mode.
    Start_Capturing();

    // A one-shot preservation run must never fall through into GUI startup.
    ExitProcess(3);
    return FALSE;
}
