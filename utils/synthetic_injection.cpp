#ifndef STRICT
#define STRICT 1
#endif

#include <windows.h>
#include <cstring>

#include "../Headers/pdw.h"
#include "../Headers/misc.h"
#include "../core/legacy_message_projection.h"
#include "../core/synthetic_injection_policy.h"
#include "../core/synthetic_message.h"
#include "synthetic_injection.h"

extern BYTE message_color[MAX_STR_LEN+1];
extern int iConvertingGroupcall;

static_assert(MAX_STR_LEN == pdw::kLegacyMessageFieldMax + 1,
              "portable legacy message bound must match Win32 MAX_STR_LEN");

namespace pdw
{
namespace
{
bool CopyBounded(const std::string& source, char* destination, std::size_t capacity)
{
    if (!destination || capacity == 0 || source.size() >= capacity)
    {
        return false;
    }

    std::memcpy(destination, source.data(), source.size());
    destination[source.size()] = '\0';
    return true;
}
}

bool InjectSyntheticMessageThroughPipeline(const SyntheticMessageRequest& request)
{
    // The legacy ShowMessage path treats group-call conversion as an internal
    // multi-message state. Synthetic injection must never mutate that state.
    if (!CanInjectSyntheticMessage(iConvertingGroupcall != 0))
    {
        return false;
    }

    DecodedMessage decoded;
    if (!BuildSyntheticMessage(request, &decoded))
    {
        return false;
    }

    LegacyMessageFields fields;
    if (!ProjectDecodedMessageToLegacyFields(decoded, &fields))
    {
        return false;
    }

    if (fields[MSG_MESSAGE].size() >= MAX_STR_LEN)
    {
        return false;
    }

    char staged[9][MAX_STR_LEN] = {};
    for (int index = MSG_CAPCODE; index <= MSG_MOBITEX; ++index)
    {
        if (!CopyBounded(fields[index], staged[index], MAX_STR_LEN))
        {
            return false;
        }
    }

    const std::size_t message_length = fields[MSG_MESSAGE].size();

    // Commit only after all validation/staging has succeeded.
    std::memcpy(Current_MSG, staged, sizeof(staged));
    std::memset(message_buffer, 0, MAX_STR_LEN + 1);
    std::memset(mobitex_buffer, 0, MAX_STR_LEN + 1);
    std::memset(message_color, COLOR_MESSAGE, MAX_STR_LEN + 1);

    if (message_length)
    {
        std::memcpy(message_buffer, fields[MSG_MESSAGE].data(), message_length);
    }
    message_buffer[message_length] = '\0';
    iMessageIndex = static_cast<int>(message_length);

    // misc.h maps this call to PreservationShowMessage(), which then enters
    // the unchanged legacy ShowMessage() path unless preservation replay is
    // explicitly running headless.
    ShowMessage();
    return true;
}

bool InjectDefaultSyntheticTestMessage()
{
    SyntheticMessageRequest request;
    request.protocol = DecodedProtocol::Pocsag;
    request.address = "1234567";
    request.text = "PDW TEST MESSAGE";
    return InjectSyntheticMessageThroughPipeline(request);
}
}
