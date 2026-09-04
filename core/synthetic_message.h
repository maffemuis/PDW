#ifndef PDW_CORE_SYNTHETIC_MESSAGE_H
#define PDW_CORE_SYNTHETIC_MESSAGE_H

#include "decoded_message.h"

#include <string>

namespace pdw
{
struct SyntheticMessageRequest
{
    SyntheticMessageRequest();

    DecodedProtocol protocol;
    std::string address;
    std::string text;
};

// Builds an explicitly synthetic DecodedMessage for local test injection.
// Returns false for invalid or unbounded input and leaves output untouched.
bool BuildSyntheticMessage(const SyntheticMessageRequest& request, DecodedMessage* output);

bool IsSyntheticMessage(const DecodedMessage& message);
}

#endif
