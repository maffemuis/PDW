#include "input_runtime_status.h"

static bool Is(bool audio, bool serial, bool running, bool paused, InputRuntimeStatus expected)
{
    return GetInputRuntimeStatus(audio, serial, running, paused) == expected;
}

int main()
{
    if (!Is(false, false, false, false, InputRuntimeStatus::NotConfigured)) return 1;
    if (!Is(false, false, false, true,  InputRuntimeStatus::NotConfigured)) return 2;
    if (!Is(true,  false, false, false, InputRuntimeStatus::Error)) return 3;
    if (!Is(false, true,  false, false, InputRuntimeStatus::Error)) return 4;
    if (!Is(true,  true,  false, true,  InputRuntimeStatus::Error)) return 5;
    if (!Is(true,  false, true,  false, InputRuntimeStatus::Running)) return 6;
    if (!Is(false, true,  true,  false, InputRuntimeStatus::Running)) return 7;
    if (!Is(false, true,  true,  true,  InputRuntimeStatus::Paused)) return 8;
    return 0;
}
