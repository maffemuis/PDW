#ifndef PDW_SYNTHETIC_INJECTION_H
#define PDW_SYNTHETIC_INJECTION_H

#include "../core/synthetic_message.h"

namespace pdw
{
// Inject a validated synthetic message through the same preservation and
// legacy ShowMessage path used by decoder-originated messages.
// Returns false before touching legacy globals when validation/projection fails.
bool InjectSyntheticMessageThroughPipeline(const SyntheticMessageRequest& request);

// Explicit user-triggered smoke message. The caller is responsible for
// confirmation/UI; this function never runs automatically.
bool InjectDefaultSyntheticTestMessage();
}

#endif
