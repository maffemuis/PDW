#include "synthetic_message.h"

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
    pdw::SyntheticMessageRequest request;
    request.protocol = pdw::DecodedProtocol::Pocsag;
    request.address = "1234567";
    request.text = "BRANDWEER TEST";

    pdw::DecodedMessage output;
    output.address = "UNCHANGED";
    if (!pdw::BuildSyntheticMessage(request, &output))
    {
        return Fail("valid synthetic request rejected");
    }
    if (!pdw::IsSyntheticMessage(output))
    {
        return Fail("synthetic marker missing");
    }
    if (output.protocol != pdw::DecodedProtocol::Pocsag
        || output.address != "1234567"
        || output.text != "BRANDWEER TEST"
        || output.mode != "TEST"
        || output.type != " SYNTHETIC "
        || output.auxiliary != "TEST/SYNTHETIC")
    {
        return Fail("synthetic message fields changed");
    }

    const pdw::DecodedMessage preserved = output;
    request.protocol = pdw::DecodedProtocol::Unknown;
    if (pdw::BuildSyntheticMessage(request, &output))
    {
        return Fail("unknown protocol must fail closed");
    }
    if (output.protocol != preserved.protocol
        || output.address != preserved.address
        || output.received_time != preserved.received_time
        || output.received_date != preserved.received_date
        || output.mode != preserved.mode
        || output.type != preserved.type
        || output.bitrate != preserved.bitrate
        || output.text != preserved.text
        || output.auxiliary != preserved.auxiliary)
    {
        return Fail("failed build must leave entire output untouched");
    }

    request.protocol = pdw::DecodedProtocol::Pocsag;
    request.address.clear();
    if (pdw::BuildSyntheticMessage(request, &output))
    {
        return Fail("empty address must fail closed");
    }

    request.address.assign(33, '1');
    if (pdw::BuildSyntheticMessage(request, &output))
    {
        return Fail("oversized address must fail closed");
    }

    request.address = "1234567";
    request.text.clear();
    if (pdw::BuildSyntheticMessage(request, &output))
    {
        return Fail("empty text must fail closed");
    }

    request.text.assign(1023, 'A');
    if (!pdw::BuildSyntheticMessage(request, &output))
    {
        return Fail("max safe legacy text length must be accepted");
    }

    const pdw::DecodedMessage max_preserved = output;
    request.text.assign(1024, 'B');
    if (pdw::BuildSyntheticMessage(request, &output))
    {
        return Fail("text beyond legacy pipeline bound must fail closed");
    }
    if (output.protocol != max_preserved.protocol
        || output.address != max_preserved.address
        || output.received_time != max_preserved.received_time
        || output.received_date != max_preserved.received_date
        || output.mode != max_preserved.mode
        || output.type != max_preserved.type
        || output.bitrate != max_preserved.bitrate
        || output.text != max_preserved.text
        || output.auxiliary != max_preserved.auxiliary)
    {
        return Fail("overlong failed build modified output");
    }

    if (pdw::BuildSyntheticMessage(request, 0))
    {
        return Fail("null output must fail closed");
    }

    return 0;
}
