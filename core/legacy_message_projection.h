#ifndef PDW_CORE_LEGACY_MESSAGE_PROJECTION_H
#define PDW_CORE_LEGACY_MESSAGE_PROJECTION_H

#include "decoded_message.h"

#include <array>
#include <cstddef>
#include <string>

namespace pdw
{
constexpr std::size_t kLegacyMessageFieldCount = 9;
constexpr std::size_t kLegacyMessageFieldMax = 5119;

using LegacyMessageFields = std::array<std::string, kLegacyMessageFieldCount>;

// Projects a DecodedMessage onto the legacy Current_MSG field layout.
// Index 0 is intentionally unused; 1..8 match MSG_* constants.
// MAX_STR_LEN is 5120 in the Win32 legacy layer, so 5119 bytes plus NUL is
// the largest field that can be projected safely.
// Returns false when any field exceeds the legacy bound; output is unchanged.
bool ProjectDecodedMessageToLegacyFields(const DecodedMessage& message, LegacyMessageFields* output);
}

#endif
