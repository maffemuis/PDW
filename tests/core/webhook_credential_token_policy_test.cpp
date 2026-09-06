#include <cassert>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#include "integration_worker.h"

namespace
{
class CountingTransport : public pdw::IWebhookTransport
{
public:
    CountingTransport() : calls_(0) {}

    bool PostJson(const pdw::WebhookDeliveryRequest&) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++calls_;
        return true;
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

bool WaitForFailed(pdw::AsyncIntegrationWorker& worker)
{
    for (int i = 0; i < 100; ++i)
    {
        if (worker.FailedCount() >= 1) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}
}

int main()
{
    CountingTransport transport;
    pdw::IntegrationWorkerOptions options;
    options.max_attempts = 1;

    pdw::AsyncIntegrationWorker worker(
        transport,
        options,
        "https://example.test/hook",
        []() { return std::string("secret\r\ninjected-header: yes"); });

    assert(worker.Start());
    assert(worker.TryEnqueue("{}"));
    assert(WaitForFailed(worker));
    worker.Stop();

    assert(transport.Calls() == 0);
    assert(worker.DeliveredCount() == 0);
    assert(worker.FailedCount() == 1);
    assert(worker.OutstandingBytes() == 0);
    return 0;
}
