#ifndef PDW_CORE_DECODED_MESSAGE_H
#define PDW_CORE_DECODED_MESSAGE_H

#include <string>

namespace pdw
{
enum class DecodedProtocol
{
    Unknown = 0,
    Pocsag,
    Flex,
    Acars,
    Mobitex,
    Ermes
};

struct DecodedMessage
{
    DecodedMessage();

    DecodedProtocol protocol;
    std::string address;
    std::string received_time;
    std::string received_date;
    std::string mode;
    std::string type;
    std::string bitrate;
    std::string text;
    std::string auxiliary;
};

const char *DecodedProtocolName(DecodedProtocol protocol);
}

#endif
