#pragma once

// Hardware-independent first-run choice used by PDW's Win32 bootstrap.
struct StartupInputSelection
{
    int audioEnabled;
    int audioDevice;
    int comPortEnabled;
    int comPort;
};

StartupInputSelection ChooseStartupInput(unsigned int audioDeviceCount, int detectedComPort);
