#include <assert.h>
#include <chrono>
#include <string>
#include <thread>

#include "../../utils/webhook_runtime.h"

namespace
{
class FakeTransport : public pdw::IWebhookTransport
{
public:
    FakeTransport() : calls(0) {}

    bool PostJson(const pdw::WebhookDeliveryRequest& request) override
    {
        ++calls;
        last_endpoint = request.endpoint_https;
        last_body = request.json_body;
        last_timeout = request.timeout_ms;
        return true;
    }

    int calls;
    std::string last_endpoint;
    std::string last_body;
    unsigned long last_timeout;
};
}

int main()
{
    FakeTransport transport;
    pdw::WebhookRuntime runtime(transport);

    pdw::WebhookRuntimeConfig disabled;
    assert(runtime.ApplyConfig(disabled));
    assert(!runtime.IsEnabled());
    assert(!runtime.TryEnqueue("{\"event\":1}"));

    pdw::WebhookRuntimeConfig invalid;
    invalid.enabled = true;
    invalid.endpoint_https = "http://example.test/pdw";
    invalid.credential_target = L"PDW/Webhook/test";
    assert(!runtime.ApplyConfig(invalid));
    assert(!runtime.IsEnabled());

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
    assert(runtime.TryEnqueue("{\"event\":2}"));

    for (int i = 0; i < 100 && transport.calls == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    assert(transport.calls == 1);
    assert(transport.last_endpoint == valid.endpoint_https);
    assert(transport.last_body == "{\"event\":2}");
    assert(transport.last_timeout == valid.request_timeout_ms);

    runtime.Stop();
    assert(!runtime.IsEnabled());
    assert(!runtime.TryEnqueue("{\"event\":3}"));
    return 0;
}
