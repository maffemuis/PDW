#include <cassert>

#include "integration_worker.h"

namespace
{
class NoopTransport : public pdw::IWebhookTransport
{
public:
    bool PostJson(const pdw::WebhookDeliveryRequest&) override
    {
        return true;
    }
};

bool StartsWith(const pdw::IntegrationWorkerOptions& options)
{
    NoopTransport transport;
    pdw::AsyncIntegrationWorker worker(
        transport, options, "https://example.test/hook",
        pdw::AsyncIntegrationWorker::CredentialProvider());
    const bool started = worker.Start();
    if (started) worker.Stop();
    return started;
}
}

int main()
{
    pdw::IntegrationWorkerOptions options;

    options.queue_capacity = 4096;
    assert(StartsWith(options));
    options.queue_capacity = 4097;
    assert(!StartsWith(options));

    options = pdw::IntegrationWorkerOptions();
    options.max_payload_bytes = 8 * 1024 * 1024;
    options.max_outstanding_bytes = 8 * 1024 * 1024;
    assert(StartsWith(options));
    options.max_payload_bytes = (8 * 1024 * 1024) + 1;
    options.max_outstanding_bytes = options.max_payload_bytes;
    assert(!StartsWith(options));

    options = pdw::IntegrationWorkerOptions();
    options.max_outstanding_bytes = 64 * 1024 * 1024;
    assert(StartsWith(options));
    options.max_outstanding_bytes = (64 * 1024 * 1024) + 1;
    assert(!StartsWith(options));

    options = pdw::IntegrationWorkerOptions();
    options.max_attempts = 10;
    assert(StartsWith(options));
    options.max_attempts = 11;
    assert(!StartsWith(options));

    options = pdw::IntegrationWorkerOptions();
    options.initial_backoff_ms = 60000UL;
    options.max_backoff_ms = 60000UL;
    assert(StartsWith(options));
    options.max_backoff_ms = 60001UL;
    assert(!StartsWith(options));

    return 0;
}
