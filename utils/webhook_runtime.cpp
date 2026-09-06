#include "webhook_runtime.h"

namespace pdw
{

WebhookRuntime::WebhookRuntime(IWebhookTransport& transport,
                               const CredentialProvider& credential_provider)
    : transport_(transport),
      credential_provider_(credential_provider)
{
}

WebhookRuntime::~WebhookRuntime()
{
    Stop();
}

std::shared_ptr<AsyncIntegrationWorker> WebhookRuntime::SnapshotWorker() const
{
    std::lock_guard<std::mutex> lock(worker_mutex_);
    return worker_;
}

std::shared_ptr<AsyncIntegrationWorker> WebhookRuntime::DetachWorker()
{
    std::lock_guard<std::mutex> lock(worker_mutex_);
    std::shared_ptr<AsyncIntegrationWorker> previous = worker_;
    worker_.reset();
    return previous;
}

bool WebhookRuntime::ApplyConfig(const WebhookRuntimeConfig& config,
                                 const IntegrationWorkerOptions& options)
{
    std::lock_guard<std::mutex> lifecycle_lock(lifecycle_mutex_);

    // Remove the old worker from decoder visibility before Stop() can wait on
    // an in-flight request. A decoder caller can then only see null or hold its
    // own short-lived shared_ptr snapshot of the old worker.
    std::shared_ptr<AsyncIntegrationWorker> previous = DetachWorker();
    if (previous) previous->Stop();

    config_ = config;
    if (!config_.IsValid()) return false;
    if (!config_.enabled) return true;

    IntegrationWorkerOptions effective = options;
    effective.request_timeout_ms = config_.request_timeout_ms;
    if (effective.queue_capacity == 0 || effective.max_attempts == 0)
        return false;

    CredentialProvider credentials = credential_provider_;
    if (!credentials)
    {
        const std::wstring credential_target = config_.credential_target;
        credentials = [credential_target]() {
            return ReadWindowsGenericCredentialUtf8(credential_target);
        };
    }

    std::shared_ptr<AsyncIntegrationWorker> next;
    try
    {
        next.reset(new AsyncIntegrationWorker(
            transport_, effective, config_.endpoint_https, credentials));
        if (!next->Start()) return false;
    }
    catch (...)
    {
        // Configuration failure must never escape into the host application.
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        worker_ = next;
    }
    return true;
}

void WebhookRuntime::Stop()
{
    std::lock_guard<std::mutex> lifecycle_lock(lifecycle_mutex_);
    std::shared_ptr<AsyncIntegrationWorker> previous = DetachWorker();
    if (previous) previous->Stop();
}

bool WebhookRuntime::TryEnqueue(const std::string& json_body)
{
    const std::shared_ptr<AsyncIntegrationWorker> worker = SnapshotWorker();
    return worker ? worker->TryEnqueue(json_body) : false;
}

bool WebhookRuntime::IsEnabled() const
{
    return static_cast<bool>(SnapshotWorker());
}

std::size_t WebhookRuntime::QueueSize() const
{
    const std::shared_ptr<AsyncIntegrationWorker> worker = SnapshotWorker();
    return worker ? worker->QueueSize() : 0;
}

std::size_t WebhookRuntime::OutstandingBytes() const
{
    const std::shared_ptr<AsyncIntegrationWorker> worker = SnapshotWorker();
    return worker ? worker->OutstandingBytes() : 0;
}

std::size_t WebhookRuntime::DroppedCount() const
{
    const std::shared_ptr<AsyncIntegrationWorker> worker = SnapshotWorker();
    return worker ? worker->DroppedCount() : 0;
}

std::size_t WebhookRuntime::DeliveredCount() const
{
    const std::shared_ptr<AsyncIntegrationWorker> worker = SnapshotWorker();
    return worker ? worker->DeliveredCount() : 0;
}

std::size_t WebhookRuntime::FailedCount() const
{
    const std::shared_ptr<AsyncIntegrationWorker> worker = SnapshotWorker();
    return worker ? worker->FailedCount() : 0;
}

} // namespace pdw
