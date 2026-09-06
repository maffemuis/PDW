#ifndef PDW_UTILS_WEBHOOK_INTEGRATION_H
#define PDW_UTILS_WEBHOOK_INTEGRATION_H

#include <cstddef>

#include "../core/decoded_message.h"

namespace pdw
{

// Decoder-facing integration seam. The default production service is disabled,
// so this remains a bounded no-op until a validated configuration is applied by
// a later settings layer. Network I/O is always owned by WebhookRuntime's worker.
bool TryPublishDecodedMessageWebhook(const DecodedMessage& message);

bool IsWebhookIntegrationEnabled();
std::size_t WebhookIntegrationQueueSize();
std::size_t WebhookIntegrationOutstandingBytes();
std::size_t WebhookIntegrationDroppedCount();
std::size_t WebhookIntegrationDeliveredCount();
std::size_t WebhookIntegrationFailedCount();

} // namespace pdw

#endif
