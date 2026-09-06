#ifndef PDW_CORE_INTEGRATION_EVENT_H
#define PDW_CORE_INTEGRATION_EVENT_H

#include <string>

#include "decoded_message.h"

namespace pdw
{

// Stable external integration contract. Bump schema_version only when the
// serialized contract changes incompatibly.
struct IntegrationMessageEvent
{
    IntegrationMessageEvent();

    int schema_version;
    std::string event_type;
    std::string protocol;
    std::string address;
    std::string received_date;
    std::string received_time;
    std::string mode;
    std::string type;
    std::string bitrate;
    std::string text;
    std::string auxiliary;
};

IntegrationMessageEvent MakeIntegrationMessageEvent(const DecodedMessage& message);
std::string SerializeIntegrationMessageEventJson(const IntegrationMessageEvent& event);

} // namespace pdw

#endif
