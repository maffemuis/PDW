#ifndef STRICT
#define STRICT 1
#endif

#include <windows.h>
#include <string.h>

#define PDW_SHOWMESSAGE_IMPLEMENTATION 1
#include "../Headers/pdw.h"
#include "../Headers/misc.h"

#include "preservation_capture.h"

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

    ShowMessage();
}
