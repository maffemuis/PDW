#include <cassert>
#include <chrono>
#include <stdexcept>
#include <thread>

#include "integration_worker.h"

namespace
{
class ThrowingTransport : public pdw::IWebhookTransport
{
public:
    bool PostJson(const pdw::WebhookDeliveryRequest&) override
    {
        throw std::runtime_error("transport failure");
    }
};

bool WaitForFailed(pdw::AsyncIntegrationWorker& worker, std::size_t count)
{
    for (int i = 0; i < 100; ++i)
    {
        if (worker.FailedCount() >= count) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}
}

int main()
{
    pdw::IntegrationWorkerOptions options;
    options.queue_capacity = 2;
    options.max_payload_bytes = 1024;
    options.max_outstanding_bytes = 2048;
    options.max_attempts = 1;
    options.initial_backoff_ms = 0;
    options.max_backoff_ms = 0;

    ThrowingTransport throwing_transport;
    pdw::AsyncIntegrationWorker transport_worker(
        throwing_transport,
        options,
        "https://example.test/hook",
        pdw::AsyncIntegrationWorker::CredentialProvider());
    assert(transport_worker.Start());
    assert(transport_worker.TryEnqueue("{}"));
    assert(WaitForFailed(transport_worker, 1));
    transport_worker.Stop();
    assert(transport_worker.DeliveredCount() == 0);
    assert(transport_worker.OutstandingBytes() == 0);

    ThrowingTransport unused_transport;
    pdw::AsyncIntegrationWorker credential_worker(
        unused_transport,
        options,
        "https://example.test/hook",
        []() -> std::string {
            throw std::runtime_error("credential failure");
        });
    assert(credential_worker.Start());
    assert(credential_worker.TryEnqueue("{}"));
    assert(WaitForFailed(credential_worker, 1));
    credential_worker.Stop();
    assert(credential_worker.DeliveredCount() == 0);
    assert(credential_worker.OutstandingBytes() == 0);

    return 0;
}
