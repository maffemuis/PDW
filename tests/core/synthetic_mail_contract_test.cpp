#include "synthetic_message.h"
#include "legacy_message_projection.h"

#include <iostream>
#include <string>

namespace
{
struct MockMailCapture
{
    bool called = false;
    std::string address;
    std::string mode;
    std::string type;
    std::string message;
};

void MockSendMail(const pdw::LegacyMessageFields& fields, MockMailCapture* capture)
{
    capture->called = true;
    capture->address = fields[1];
    capture->mode = fields[4];
    capture->type = fields[5];
    capture->message = fields[7];
}

int Fail(const char* message)
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
    request.text = "PDW TEST MESSAGE";

    pdw::DecodedMessage decoded;
    if (!pdw::BuildSyntheticMessage(request, &decoded))
        return Fail("synthetic message build failed");

    pdw::LegacyMessageFields fields;
    if (!pdw::ProjectDecodedMessageToLegacyFields(decoded, &fields))
        return Fail("synthetic legacy projection failed");

    MockMailCapture capture;
    MockSendMail(fields, &capture);

    if (!capture.called)
        return Fail("mock mail action was not invoked");
    if (capture.address != "1234567")
        return Fail("synthetic capcode changed before mail action");
    if (capture.mode != "TEST")
        return Fail("TEST marker missing from mail action input");
    if (capture.type != " SYNTHETIC ")
        return Fail("SYNTHETIC marker missing from mail action input");
    if (capture.message != "PDW TEST MESSAGE")
        return Fail("synthetic message changed before mail action");

    return 0;
}
