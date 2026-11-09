#include "startup_input_selection.h"

StartupInputSelection ChooseStartupInput(unsigned int audioDeviceCount, int detectedComPort)
{
    StartupInputSelection selection = {};
    selection.audioEnabled = audioDeviceCount > 0 ? 1 : 0;
    selection.audioDevice = 0; // First waveIn device ID, not a combo-box row.
    selection.comPortEnabled = (audioDeviceCount == 0 && detectedComPort > 0) ? 1 : 0;
    selection.comPort = detectedComPort > 0 ? detectedComPort : 2;
    return selection;
}
