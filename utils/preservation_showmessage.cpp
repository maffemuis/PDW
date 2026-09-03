#ifndef STRICT
#define STRICT 1
#endif

#include <windows.h>
#include <stdlib.h>
#include <string.h>

#define PDW_SHOWMESSAGE_IMPLEMENTATION 1
#include "../Headers/pdw.h"
#include "../Headers/misc.h"

#include "preservation_capture.h"

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

void PreservationShowMessage(void)
{
    char message[MAX_STR_LEN];
    int message_length = iMessageIndex;

    if (message_length < 0)
    {
        message_length = 0;
    }
    else if (message_length >= MAX_STR_LEN)
    {
        message_length = MAX_STR_LEN - 1;
    }

    if (message_length > 0)
    {
        memcpy(message, message_buffer, (size_t)message_length);
    }
    message[message_length] = '\0';

    PreservationCaptureMessage(
        Current_MSG[MSG_CAPCODE],
        Current_MSG[MSG_TIME],
        Current_MSG[MSG_DATE],
        Current_MSG[MSG_MODE],
        Current_MSG[MSG_TYPE],
        Current_MSG[MSG_BITRATE],
        message,
        Current_MSG[MSG_MOBITEX],
        MAX_STR_LEN);

    if (PreservationOneShotReplayRequested())
    {
        // Golden preservation deliberately stops at the decoder-to-ShowMessage
        // boundary. Do not enter legacy GUI/filter/logging side effects on a
        // headless runner; just prepare the message accumulator for another
        // decoded message in the same recording.
        iMessageIndex = 0;
        message_buffer[0] = '\0';
        mobitex_buffer[0] = '\0';
        return;
    }

    ShowMessage();
}
