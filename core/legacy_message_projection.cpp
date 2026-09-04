#include "legacy_message_projection.h"

namespace pdw
{
namespace
{
bool FitsLegacyField(const std::string& value)
{
    return value.size() <= kLegacyMessageFieldMax;
}
}

bool ProjectDecodedMessageToLegacyFields(const DecodedMessage& message, LegacyMessageFields* output)
{
    if (output == nullptr)
    {
        return false;
    }

    const std::string* values[] = {
        &message.address,
        &message.received_time,
        &message.received_date,
        &message.mode,
        &message.type,
        &message.bitrate,
        &message.text,
        &message.auxiliary
    };

    for (const std::string* value : values)
    {
        if (!FitsLegacyField(*value))
        {
            return false;
        }
    }

    LegacyMessageFields projected{};
    projected[1] = message.address;
    projected[2] = message.received_time;
    projected[3] = message.received_date;
    projected[4] = message.mode;
    projected[5] = message.type;
    projected[6] = message.bitrate;
    projected[7] = message.text;
    projected[8] = message.auxiliary;

    *output = projected;
    return true;
}
}
