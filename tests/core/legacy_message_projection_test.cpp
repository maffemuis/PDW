#include "legacy_message_projection.h"

#include <iostream>
#include <string>

namespace
{
int Fail(const char* message)
{
    std::cerr << message << std::endl;
    return 1;
}
}

int main()
{
    pdw::DecodedMessage message;
    message.address = "1234567";
    message.received_time = "12:34:56";
    message.received_date = "04-09-26";
    message.mode = "POCSAG-1";
    message.type = " ALPHA ";
    message.bitrate = "1200";
    message.text = "TEST SYNTHETIC BRANDWEER";
    message.auxiliary = "SYNTHETIC";

    pdw::LegacyMessageFields fields;
    fields[1] = "sentinel";

    if (!pdw::ProjectDecodedMessageToLegacyFields(message, &fields))
    {
        return Fail("valid decoded message projection failed");
    }

    if (!fields[0].empty()
        || fields[1] != message.address
        || fields[2] != message.received_time
        || fields[3] != message.received_date
        || fields[4] != message.mode
        || fields[5] != message.type
        || fields[6] != message.bitrate
        || fields[7] != message.text
        || fields[8] != message.auxiliary)
    {
        return Fail("legacy field layout mismatch");
    }

    pdw::LegacyMessageFields unchanged;
    unchanged[1] = "unchanged";
    pdw::LegacyMessageFields before = unchanged;

    message.text.assign(pdw::kLegacyMessageFieldMax + 1, 'X');
    if (pdw::ProjectDecodedMessageToLegacyFields(message, &unchanged))
    {
        return Fail("overlong message must fail closed");
    }
    if (unchanged != before)
    {
        return Fail("failed projection modified output");
    }

    message.text.assign(pdw::kLegacyMessageFieldMax, 'Y');
    if (!pdw::ProjectDecodedMessageToLegacyFields(message, &unchanged))
    {
        return Fail("max-bounded message must be accepted");
    }

    if (pdw::ProjectDecodedMessageToLegacyFields(message, nullptr))
    {
        return Fail("null projection target must fail closed");
    }

    return 0;
}
