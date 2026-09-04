#include "synthetic_message.h"
#include "legacy_message_projection.h"

namespace pdw
{
namespace
{
const std::size_t kSyntheticAddressMax = 32;
const std::size_t kSyntheticTextMax = kLegacyMessageFieldMax;
}

SyntheticMessageRequest::SyntheticMessageRequest()
    : protocol(DecodedProtocol::Unknown)
{
}

bool BuildSyntheticMessage(const SyntheticMessageRequest& request, DecodedMessage* output)
{
    if (output == 0)
    {
        return false;
    }

    if (request.protocol == DecodedProtocol::Unknown
        || request.address.empty()
        || request.address.size() > kSyntheticAddressMax
        || request.text.empty()
        || request.text.size() > kSyntheticTextMax)
    {
        return false;
    }

    DecodedMessage candidate;
    candidate.protocol = request.protocol;
    candidate.address = request.address;
    candidate.mode = "TEST";
    candidate.type = " SYNTHETIC ";
    candidate.text = request.text;
    candidate.auxiliary = "TEST/SYNTHETIC";

    *output = candidate;
    return true;
}

bool IsSyntheticMessage(const DecodedMessage& message)
{
    return message.mode == "TEST"
        && message.type == " SYNTHETIC "
        && message.auxiliary == "TEST/SYNTHETIC";
}
}
