#include "synthetic_message.h"
#include "legacy_message_projection.h"

#include <iostream>
#include <string>

namespace
{
int Fail(const char *message)
{
    std::cerr << message << std::endl;
    return 1;
}

bool SameMessage(const pdw::DecodedMessage& left, const pdw::DecodedMessage& right)
{
    return left.protocol == right.protocol
        && left.address == right.address
        && left.received_time == right.received_time
        && left.received_date == right.received_date
        && left.mode == right.mode
        && left.type == right.type
        && left.bitrate == right.bitrate
        && left.text == right.text
        && left.auxiliary == right.auxiliary;
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
    if (!SameMessage(output, preserved))
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

    request.text.assign(pdw::kLegacyMessageFieldMax, 'A');
    if (!pdw::BuildSyntheticMessage(request, &output))
    {
        return Fail("5119-byte legacy-safe text must be accepted");
    }
    if (output.text.size() != pdw::kLegacyMessageFieldMax)
    {
        return Fail("max safe synthetic text length changed");
    }

    const pdw::DecodedMessage max_preserved = output;
    request.text.assign(pdw::kLegacyMessageFieldMax + 1, 'B');
    if (pdw::BuildSyntheticMessage(request, &output))
    {
        return Fail("5120-byte text must fail closed");
    }
    if (!SameMessage(output, max_preserved))
    {
        return Fail("overlong failed build modified output");
    }

    if (pdw::BuildSyntheticMessage(request, 0))
    {
        return Fail("null output must fail closed");
    }

    return 0;
}
