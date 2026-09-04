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
    if (output.address != preserved.address || output.text != preserved.text)
    {
        return Fail("failed build must leave output untouched");
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

    request.text.assign(1025, 'A');
    if (pdw::BuildSyntheticMessage(request, &output))
    {
        return Fail("oversized text must fail closed");
    }

    if (pdw::BuildSyntheticMessage(request, 0))
    {
        return Fail("null output must fail closed");
    }

    return 0;
}
