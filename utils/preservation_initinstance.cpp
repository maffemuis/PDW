#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "..\Headers\initapp.h"
#include "..\Headers\sound_in.h"

HWND NEAR LegacyInitInstance(HINSTANCE hInstance, int nCmdShow);

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

HWND NEAR InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    HWND hwnd = LegacyInitInstance(hInstance, nCmdShow);

    if (!hwnd || !PreservationOneShotReplayRequested())
    {
        return hwnd;
    }

    // The profile has already been loaded by InitApplication and the legacy
    // window/panes now exist. Run one-shot preservation before unrelated
    // legacy startup work (database reads, COM discovery, timers, message loop).
    // Start_Capturing() terminates with exit 0/2 in one-shot replay mode.
    Start_Capturing();

    // One-shot replay must never fall through into the GUI runtime.
    ExitProcess(3);
    return hwnd;
}
