#include "decoded_message.h"

namespace pdw
{
DecodedMessage::DecodedMessage()
    : protocol(DecodedProtocol::Unknown)
{
}

const char *DecodedProtocolName(DecodedProtocol protocol)
{
    switch (protocol)
    {
        case DecodedProtocol::Pocsag:
            return "pocsag";
        case DecodedProtocol::Flex:
            return "flex";
        case DecodedProtocol::Acars:
            return "acars";
        case DecodedProtocol::Mobitex:
            return "mobitex";
        case DecodedProtocol::Ermes:
            return "ermes";
        case DecodedProtocol::Unknown:
        default:
            return "unknown";
    }
}
}
