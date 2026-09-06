#include <assert.h>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#include "../../utils/webhook_runtime.h"

namespace
{
class FakeTransport : public pdw::IWebhookTransport
{
public:
    FakeTransport() : calls_(0), last_timeout_(0) {}

    bool PostJson(const pdw::WebhookDeliveryRequest& request) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++calls_;
        last_endpoint_ = request.endpoint_https;
        last_body_ = request.json_body;
        last_timeout_ = request.timeout_ms;
        return true;
    }

    int Calls() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return calls_;
    }

    std::string LastEndpoint() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_endpoint_;
    }

    std::string LastBody() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_body_;
    }

    unsigned long LastTimeout() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_timeout_;
    }

private:
    mutable std::mutex mutex_;
    int calls_;
    std::string last_endpoint_;
    std::string last_body_;
    unsigned long last_timeout_;
};
}

int main()
{
    FakeTransport transport;
    pdw::WebhookRuntime runtime(transport);

    pdw::WebhookRuntimeConfig disabled;
    assert(runtime.ApplyConfig(disabled));
    assert(!runtime.IsEnabled());
    assert(runtime.OutstandingBytes() == 0);
    assert(!runtime.TryEnqueue("{\"event\":1}"));

    pdw::WebhookRuntimeConfig invalid;
    invalid.enabled = true;
    invalid.endpoint_https = "http://example.test/pdw";
    invalid.credential_target = L"PDW/Webhook/test";
    assert(!runtime.ApplyConfig(invalid));
    assert(!runtime.IsEnabled());
    assert(runtime.OutstandingBytes() == 0);

    pdw::WebhookRuntimeConfig valid;
    valid.enabled = true;
    valid.endpoint_https = "https://example.test/pdw";
    valid.credential_target = L"PDW/Webhook/test-runtime-missing-credential";
    valid.request_timeout_ms = 3210;

    pdw::IntegrationWorkerOptions options;
    options.queue_capacity = 2;
    options.max_attempts = 1;
    options.initial_backoff_ms = 0;
    options.max_backoff_ms = 0;

    assert(runtime.ApplyConfig(valid, options));
    assert(runtime.IsEnabled());
    assert(runtime.OutstandingBytes() == 0);
    assert(runtime.TryEnqueue("{\"event\":2}"));
    assert(runtime.OutstandingBytes() <= options.max_outstanding_bytes);

    for (int i = 0; i < 100 && transport.Calls() == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    assert(transport.Calls() == 1);
    assert(transport.LastEndpoint() == valid.endpoint_https);
    assert(transport.LastBody() == "{\"event\":2}");
    assert(transport.LastTimeout() == valid.request_timeout_ms);

    runtime.Stop();
    assert(!runtime.IsEnabled());
    assert(runtime.OutstandingBytes() == 0);
    assert(!runtime.TryEnqueue("{\"event\":3}"));
    return 0;
}
