#include <assert.h>

#include "../../utils/webhook_integration.h"

int main()
{
    pdw::DecodedMessage message;
    message.protocol = pdw::DecodedProtocol::Pocsag;
    message.address = "1234567";
    message.received_date = "2026-09-06";
    message.received_time = "23:59:59";
    message.mode = "POCSAG";
    message.type = "ALPHA";
    message.bitrate = "1200";
    message.text = "disabled integration must stay fail-open";

    // Production wiring is deliberately disabled until a validated settings
    // layer applies configuration. Merely decoding a message must not enqueue,
    // drop, deliver, fail, or perform network I/O.
    assert(!pdw::IsWebhookIntegrationEnabled());
    assert(pdw::WebhookIntegrationQueueSize() == 0);
    assert(pdw::WebhookIntegrationOutstandingBytes() == 0);
    assert(!pdw::TryPublishDecodedMessageWebhook(message));
    assert(pdw::WebhookIntegrationQueueSize() == 0);
    assert(pdw::WebhookIntegrationOutstandingBytes() == 0);
    assert(pdw::WebhookIntegrationDroppedCount() == 0);
    assert(pdw::WebhookIntegrationDeliveredCount() == 0);
    assert(pdw::WebhookIntegrationFailedCount() == 0);
    return 0;
}
