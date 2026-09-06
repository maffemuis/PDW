#include <assert.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "../../utils/webhook_runtime.h"

namespace
{
class BlockingTransport : public pdw::IWebhookTransport
{
public:
    BlockingTransport() : calls_(0), entered_(false), released_(false) {}

    bool PostJson(const pdw::WebhookDeliveryRequest&) override
    {
        std::unique_lock<std::mutex> lock(mutex_);
        ++calls_;
        entered_ = true;
        wake_.notify_all();
        wake_.wait(lock, [this]() { return released_; });
        return true;
    }

    bool WaitUntilEntered()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return wake_.wait_for(
            lock, std::chrono::seconds(2), [this]() { return entered_; });
    }

    void Release()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        released_ = true;
        wake_.notify_all();
    }

    int Calls() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return calls_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    int calls_;
    bool entered_;
    bool released_;
};
}

int main()
{
    BlockingTransport transport;
    pdw::WebhookRuntime runtime(
        transport,
        []() { return std::string("reconfigure-test-token"); });

    pdw::WebhookRuntimeConfig enabled;
    enabled.enabled = true;
    enabled.endpoint_https = "https://example.test/pdw";
    enabled.credential_target = L"PDW/Webhook/reconfigure-test";
    enabled.request_timeout_ms = 1000;

    pdw::IntegrationWorkerOptions options;
    options.queue_capacity = 4;
    options.max_attempts = 1;
    options.initial_backoff_ms = 0;
    options.max_backoff_ms = 0;

    assert(runtime.ApplyConfig(enabled, options));
    assert(runtime.TryEnqueue("{\"event\":1}"));
    assert(transport.WaitUntilEntered());

    pdw::WebhookRuntimeConfig disabled;
    bool reconfigured = false;
    std::thread reconfigure([&]() {
        reconfigured = runtime.ApplyConfig(disabled, options);
    });

    // ApplyConfig must detach the old worker before waiting for its in-flight
    // transport call. This is the decoder non-blocking guarantee needed by a
    // future settings UI.
    bool detached = false;
    for (int i = 0; i < 200; ++i)
    {
        if (!runtime.IsEnabled())
        {
            detached = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (detached)
    {
        assert(!runtime.TryEnqueue("{\"event\":2}"));
        assert(runtime.QueueSize() == 0);
        assert(runtime.OutstandingBytes() == 0);
    }

    // Always release before asserting so a failing implementation cannot leave
    // the reconfiguration thread blocked inside Stop().
    transport.Release();
    reconfigure.join();

    assert(detached);
    assert(reconfigured);
    assert(!runtime.IsEnabled());
    assert(transport.Calls() == 1);
    return 0;
}
