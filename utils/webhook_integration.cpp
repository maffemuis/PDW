#include "webhook_integration.h"

#include "../core/integration_event.h"
#include "webhook_runtime.h"

namespace pdw
{
namespace
{
class WebhookIntegrationService
{
public:
    WebhookIntegrationService()
        : runtime_(transport_)
    {
    }

    bool TryPublish(const DecodedMessage& message)
    {
        if (!runtime_.IsEnabled()) return false;

        try
        {
            const IntegrationMessageEvent event = MakeIntegrationMessageEvent(message);
            return runtime_.TryEnqueue(SerializeIntegrationMessageEventJson(event));
        }
        catch (...)
        {
            // External integration is strictly fail-open for decoder operation.
            return false;
        }
    }

    bool IsEnabled() const { return runtime_.IsEnabled(); }
    std::size_t QueueSize() const { return runtime_.QueueSize(); }
    std::size_t OutstandingBytes() const { return runtime_.OutstandingBytes(); }
    std::size_t DroppedCount() const { return runtime_.DroppedCount(); }
    std::size_t DeliveredCount() const { return runtime_.DeliveredCount(); }
    std::size_t FailedCount() const { return runtime_.FailedCount(); }

private:
    // Declaration order matters: runtime must be destroyed before its transport.
    WinHttpWebhookTransport transport_;
    WebhookRuntime runtime_;
};

WebhookIntegrationService& IntegrationService()
{
    static WebhookIntegrationService service;
    return service;
}
} // namespace

bool TryPublishDecodedMessageWebhook(const DecodedMessage& message)
{
    return IntegrationService().TryPublish(message);
}

bool IsWebhookIntegrationEnabled()
{
    return IntegrationService().IsEnabled();
}

std::size_t WebhookIntegrationQueueSize()
{
    return IntegrationService().QueueSize();
}

std::size_t WebhookIntegrationOutstandingBytes()
{
    return IntegrationService().OutstandingBytes();
}

std::size_t WebhookIntegrationDroppedCount()
{
    return IntegrationService().DroppedCount();
}

std::size_t WebhookIntegrationDeliveredCount()
{
    return IntegrationService().DeliveredCount();
}

std::size_t WebhookIntegrationFailedCount()
{
    return IntegrationService().FailedCount();
}

} // namespace pdw
