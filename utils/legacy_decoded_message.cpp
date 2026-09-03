#ifndef STRICT
#define STRICT 1
#endif

#include <windows.h>
#include <stddef.h>
#include <string.h>

#include "../Headers/pdw.h"
#include "../Headers/misc.h"
#include "legacy_decoded_message.h"

namespace
{
std::string BoundedLegacyString(const char *value)
{
    if (!value)
    {
        return std::string();
    }

    size_t length = 0;
    while (length < MAX_STR_LEN && value[length] != '\0')
    {
        ++length;
    }

    return std::string(value, length);
}

pdw::DecodedProtocol DetectLegacyProtocol()
{
    if (Profile.monitor_acars)
    {
        return pdw::DecodedProtocol::Acars;
    }

    if (Profile.monitor_mobitex)
    {
        return pdw::DecodedProtocol::Mobitex;
    }

    if (Profile.monitor_ermes)
    {
        return pdw::DecodedProtocol::Ermes;
    }

    const char *mode = Current_MSG[MSG_MODE];
    if (mode)
    {
        if (strncmp(mode, "POCSAG", 6) == 0)
        {
            return pdw::DecodedProtocol::Pocsag;
        }

        if (strncmp(mode, "FLEX", 4) == 0)
        {
            return pdw::DecodedProtocol::Flex;
        }
    }

    return pdw::DecodedProtocol::Unknown;
}
}

namespace pdw
{
DecodedMessage SnapshotLegacyDecodedMessage()
{
    DecodedMessage message;
    message.protocol = DetectLegacyProtocol();
    message.address = BoundedLegacyString(Current_MSG[MSG_CAPCODE]);
    message.received_time = BoundedLegacyString(Current_MSG[MSG_TIME]);
    message.received_date = BoundedLegacyString(Current_MSG[MSG_DATE]);
    message.mode = BoundedLegacyString(Current_MSG[MSG_MODE]);
    message.type = BoundedLegacyString(Current_MSG[MSG_TYPE]);
    message.bitrate = BoundedLegacyString(Current_MSG[MSG_BITRATE]);
    message.auxiliary = BoundedLegacyString(Current_MSG[MSG_MOBITEX]);

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
        message.text.assign(
            reinterpret_cast<const char *>(message_buffer),
            static_cast<size_t>(message_length));
    }

    return message;
}
}
