#pragma once

// The status bar must reflect whether an input really started, not only the
// user's saved preferences. Pure logic allows hardware-independent regression.
enum class InputRuntimeStatus
{
    NotConfigured,
    Error,
    Paused,
    Running
};

inline InputRuntimeStatus GetInputRuntimeStatus(
    bool audioConfigured, bool serialConfigured, bool receiverRunning, bool paused)
{
    if (!receiverRunning)
        return (audioConfigured || serialConfigured)
            ? InputRuntimeStatus::Error : InputRuntimeStatus::NotConfigured;
    return paused ? InputRuntimeStatus::Paused : InputRuntimeStatus::Running;
}
