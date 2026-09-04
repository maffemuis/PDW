#include "decoded_message.h"

#include <iostream>
#include <string>

namespace
{
int Fail(const char *message)
{
    std::cerr << message << std::endl;
    return 1;
}
}

int main()
{
    pdw::DecodedMessage message;

    if (message.protocol != pdw::DecodedProtocol::Unknown)
    {
        return Fail("default protocol must be unknown");
    }

    message.protocol = pdw::DecodedProtocol::Flex;
    message.address = "0123456";
    message.received_time = "12:34:56";
    message.received_date = "03-09-26";
    message.mode = "FLEX-A";
    message.type = " ALPHA ";
    message.bitrate = "1600";
    const char raw_text[] = {'A', 0x17, 'B'};
    message.text.assign(raw_text, 3);
    message.auxiliary = "meta";

    if (std::string(pdw::DecodedProtocolName(message.protocol)) != "flex")
    {
        return Fail("FLEX protocol name mismatch");
    }

    if (message.address != "0123456"
        || message.mode != "FLEX-A"
        || message.type != " ALPHA "
        || message.bitrate != "1600")
    {
        return Fail("decoded message fields changed");
    }

    if (message.text.size() != 3
        || message.text[0] != 'A'
        || static_cast<unsigned char>(message.text[1]) != 0x17
        || message.text[2] != 'B')
    {
        return Fail("decoded message must preserve legacy control bytes");
    }

    const pdw::DecodedProtocol protocols[] = {
        pdw::DecodedProtocol::Unknown,
        pdw::DecodedProtocol::Pocsag,
        pdw::DecodedProtocol::Flex,
        pdw::DecodedProtocol::Acars,
        pdw::DecodedProtocol::Mobitex,
        pdw::DecodedProtocol::Ermes
    };

    const char *names[] = {
        "unknown",
        "pocsag",
        "flex",
        "acars",
        "mobitex",
        "ermes"
    };

    for (size_t i = 0; i < sizeof(protocols) / sizeof(protocols[0]); ++i)
    {
        if (std::string(pdw::DecodedProtocolName(protocols[i])) != names[i])
        {
            return Fail("protocol name contract mismatch");
        }
    }

    return 0;
}
