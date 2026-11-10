#include "startup_input_selection.h"

static bool Matches(unsigned int audioCount, int detectedComPort,
                    int expectAudio, int expectSerial, int expectPort)
{
    const StartupInputSelection input = ChooseStartupInput(audioCount, detectedComPort);
    return input.audioEnabled == expectAudio &&
           input.audioDevice == 0 &&
           input.comPortEnabled == expectSerial &&
           input.comPort == expectPort;
}

int main()
{
    if (!Matches(1, 0, 1, 0, 2)) return 1;    // Audio only
    if (!Matches(3, 12, 1, 0, 12)) return 2;  // Prefer audio over COM
    if (!Matches(0, 7, 0, 1, 7)) return 3;    // COM fallback
    if (!Matches(0, 256, 0, 1, 256)) return 4;
    if (!Matches(0, 0, 0, 0, 2)) return 5;    // No input
    if (!Matches(0, -1, 0, 0, 2)) return 6;
    return 0;
}
