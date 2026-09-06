#include "webhook_runtime.h"

namespace pdw
{

WebhookRuntime::WebhookRuntime(IWebhookTransport& transport)
    : transport_(transport)
{
}

WebhookRuntime::~WebhookRuntime()
{
    Stop();
}

bool WebhookRuntime::ApplyConfig(const WebhookRuntimeConfig& config,
                                 const IntegrationWorkerOptions& options)
{
    Stop();
    config_ = config;

    if (!config_.IsValid()) return false;
    if (!config_.enabled) return true;

    IntegrationWorkerOptions effective = options;
    effective.request_timeout_ms = config_.request_timeout_ms;
    if (effective.queue_capacity == 0 || effective.max_attempts == 0)
        return false;

    const std::wstring credential_target = config_.credential_target;
    AsyncIntegrationWorker::CredentialProvider credentials =
        [credential_target]() {
            return ReadWindowsGenericCredentialUtf8(credential_target);
        };

    worker_.reset(new AsyncIntegrationWorker(
        transport_, effective, config_.endpoint_https, credentials));
    if (!worker_->Start())
    {
        worker_.reset();
        return false;
    }
    return true;
}

void WebhookRuntime::Stop()
{
    if (worker_)
    {
        worker_->Stop();
        worker_.reset();
    }
}

bool WebhookRuntime::TryEnqueue(const std::string& json_body)
{
    return worker_ ? worker_->TryEnqueue(json_body) : false;
}

bool WebhookRuntime::IsEnabled() const
{
    return worker_.get() != NULL;
}

std::size_t WebhookRuntime::QueueSize() const
{
    return worker_ ? worker_->QueueSize() : 0;
}

std::size_t WebhookRuntime::OutstandingBytes() const
{
    return worker_ ? worker_->OutstandingBytes() : 0;
}

std::size_t WebhookRuntime::DroppedCount() const
{
    return worker_ ? worker_->DroppedCount() : 0;
}

std::size_t WebhookRuntime::DeliveredCount() const
{
    return worker_ ? worker_->DeliveredCount() : 0;
}

std::size_t WebhookRuntime::FailedCount() const
{
    return worker_ ? worker_->FailedCount() : 0;
}

} // namespace pdw
