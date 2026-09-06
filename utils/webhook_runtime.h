#ifndef PDW_UTILS_WEBHOOK_RUNTIME_H
#define PDW_UTILS_WEBHOOK_RUNTIME_H

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>

#include "webhook_transport_winhttp.h"

namespace pdw
{

// Owns the async webhook worker without exposing network I/O to decoder callers.
// Applying a disabled configuration is a successful no-op; invalid enabled
// configurations fail closed at the integration boundary only.
class WebhookRuntime
{
public:
    typedef AsyncIntegrationWorker::CredentialProvider CredentialProvider;

    explicit WebhookRuntime(
        IWebhookTransport& transport,
        const CredentialProvider& credential_provider = CredentialProvider());
    ~WebhookRuntime();

    bool ApplyConfig(const WebhookRuntimeConfig& config,
                     const IntegrationWorkerOptions& options = IntegrationWorkerOptions());
    void Stop();

    // Fail-open decoder contract: this is a bounded, non-network enqueue only.
    bool TryEnqueue(const std::string& json_body);

    bool IsEnabled() const;
    std::size_t QueueSize() const;
    std::size_t OutstandingBytes() const;
    std::size_t DroppedCount() const;
    std::size_t DeliveredCount() const;
    std::size_t FailedCount() const;

private:
    WebhookRuntime(const WebhookRuntime&);
    WebhookRuntime& operator=(const WebhookRuntime&);

    std::shared_ptr<AsyncIntegrationWorker> SnapshotWorker() const;
    std::shared_ptr<AsyncIntegrationWorker> DetachWorker();

    IWebhookTransport& transport_;
    CredentialProvider credential_provider_;
    WebhookRuntimeConfig config_;

    // Lifecycle work may wait for an in-flight HTTP request, but decoder-facing
    // methods never acquire this mutex. worker_mutex_ is held only long enough
    // to copy/swap a shared_ptr.
    std::mutex lifecycle_mutex_;
    mutable std::mutex worker_mutex_;
    std::shared_ptr<AsyncIntegrationWorker> worker_;
};

} // namespace pdw

#endif
