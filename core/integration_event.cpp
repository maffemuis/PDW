#include "integration_event.h"

#include <sstream>

namespace pdw
{
namespace
{
std::string JsonEscape(const std::string& value)
{
    std::ostringstream out;
    for (std::string::const_iterator it = value.begin(); it != value.end(); ++it)
    {
        const unsigned char ch = static_cast<unsigned char>(*it);
        switch (ch)
        {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (ch < 0x20)
            {
                static const char hex[] = "0123456789abcdef";
                out << "\\u00" << hex[(ch >> 4) & 0x0f] << hex[ch & 0x0f];
            }
            else
            {
                out << static_cast<char>(ch);
            }
            break;
        }
    }
    return out.str();
}

void AppendField(std::ostringstream& out, const char* name, const std::string& value, bool comma)
{
    if (comma) out << ',';
    out << '"' << name << "\":\"" << JsonEscape(value) << '"';
}
} // namespace

IntegrationMessageEvent::IntegrationMessageEvent()
    : schema_version(1), event_type("pdw.message.decoded")
{
}

IntegrationMessageEvent MakeIntegrationMessageEvent(const DecodedMessage& message)
{
    IntegrationMessageEvent event;
    event.protocol = DecodedProtocolName(message.protocol);
    event.address = message.address;
    event.received_date = message.received_date;
    event.received_time = message.received_time;
    event.mode = message.mode;
    event.type = message.type;
    event.bitrate = message.bitrate;
    event.text = message.text;
    event.auxiliary = message.auxiliary;
    return event;
}

std::string SerializeIntegrationMessageEventJson(const IntegrationMessageEvent& event)
{
    std::ostringstream out;
    out << '{';
    out << "\"schema_version\":" << event.schema_version;
    AppendField(out, "event_type", event.event_type, true);
    AppendField(out, "protocol", event.protocol, true);
    AppendField(out, "address", event.address, true);
    AppendField(out, "received_date", event.received_date, true);
    AppendField(out, "received_time", event.received_time, true);
    AppendField(out, "mode", event.mode, true);
    AppendField(out, "type", event.type, true);
    AppendField(out, "bitrate", event.bitrate, true);
    AppendField(out, "text", event.text, true);
    AppendField(out, "auxiliary", event.auxiliary, true);
    out << '}';
    return out.str();
}

} // namespace pdw
