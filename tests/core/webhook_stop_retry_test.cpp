#include <cassert>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#include "integration_worker.h"

namespace
{
class FailingTransport : public pdw::IWebhookTransport
{
public:
    FailingTransport() : calls_(0) {}

    bool PostJson(const pdw::WebhookDeliveryRequest&) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++calls_;
        return false;
    }

    int Calls() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return calls_;
    }

private:
    mutable std::mutex mutex_;
    int calls_;
};

bool WaitForCalls(const FailingTransport& transport, int count)
{
    for (int i = 0; i < 100; ++i)
    {
        if (transport.Calls() >= count) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}
}

int main()
{
    FailingTransport transport;
    pdw::IntegrationWorkerOptions options;
    options.queue_capacity = 2;
    options.max_payload_bytes = 1024;
    options.max_outstanding_bytes = 4096;
    options.max_attempts = 3;
    options.request_timeout_ms = 1000;
    options.initial_backoff_ms = 800;
    options.max_backoff_ms = 800;

    pdw::AsyncIntegrationWorker worker(
        transport,
        options,
        "https://example.test/hook",
        pdw::AsyncIntegrationWorker::CredentialProvider());

    assert(worker.Start());
    assert(worker.TryEnqueue("retry"));
    assert(WaitForCalls(transport, 1));

    const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
    worker.Stop();
    const long long elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();

    assert(elapsed_ms < 400);
    assert(transport.Calls() == 1);
    assert(worker.FailedCount() == 1);
    assert(worker.OutstandingBytes() == 0);
    return 0;
}
