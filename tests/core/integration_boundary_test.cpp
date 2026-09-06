#include <cassert>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "decoded_message.h"
#include "integration_event.h"
#include "integration_worker.h"

namespace
{
class FakeTransport : public pdw::IWebhookTransport
{
public:
    FakeTransport() : failures_before_success(0), calls(0) {}

    bool PostJson(const pdw::WebhookDeliveryRequest& request) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        ++calls;
        requests.push_back(request);
        if (failures_before_success > 0)
        {
            --failures_before_success;
            return false;
        }
        return true;
    }

    int failures_before_success;
    int calls;
    std::vector<pdw::WebhookDeliveryRequest> requests;
    std::mutex mutex;
};

bool WaitForDelivered(pdw::AsyncIntegrationWorker& worker, std::size_t count)
{
    for (int i = 0; i < 100; ++i)
    {
        if (worker.DeliveredCount() >= count) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}
}

int main()
{
    pdw::DecodedMessage message;
    message.protocol = pdw::DecodedProtocol::Pocsag;
    message.address = "1234567";
    message.received_date = "2026-09-06";
    message.received_time = "11:30:00";
    message.mode = "POCSAG";
    message.type = "ALPHA";
    message.bitrate = "1200";
    message.text = "Brand \"A\"\nregel 2";
    message.auxiliary = "prio";

    const pdw::IntegrationMessageEvent event = pdw::MakeIntegrationMessageEvent(message);
    assert(event.schema_version == 1);
    assert(event.event_type == "pdw.message.decoded");
    assert(event.protocol == "POCSAG");

    const std::string json = pdw::SerializeIntegrationMessageEventJson(event);
    assert(json.find("\"schema_version\":1") != std::string::npos);
    assert(json.find("\"event_type\":\"pdw.message.decoded\"") != std::string::npos);
    assert(json.find("Brand \\\"A\\\"\\nregel 2") != std::string::npos);

    assert(pdw::AsyncIntegrationWorker::IsSafeHttpsEndpoint("https://example.test/hook"));
    assert(!pdw::AsyncIntegrationWorker::IsSafeHttpsEndpoint("http://example.test/hook"));
    assert(!pdw::AsyncIntegrationWorker::IsSafeHttpsEndpoint("https://"));
    assert(!pdw::AsyncIntegrationWorker::IsSafeHttpsEndpoint("https://example.test/a b"));

    FakeTransport transport;
    transport.failures_before_success = 1;

    pdw::IntegrationWorkerOptions options;
    options.queue_capacity = 2;
    options.max_attempts = 3;
    options.request_timeout_ms = 3210;
    options.initial_backoff_ms = 1;
    options.max_backoff_ms = 2;

    int credential_reads = 0;
    pdw::AsyncIntegrationWorker worker(
        transport,
        options,
        "https://example.test/hook",
        [&credential_reads]() {
            ++credential_reads;
            return std::string("secret-token");
        });

    assert(worker.Start());
    assert(worker.TryEnqueue(json));
    assert(WaitForDelivered(worker, 1));
    worker.Stop();

    assert(transport.calls == 2);
    assert(credential_reads == 2);
    assert(transport.requests.size() == 2);
    assert(transport.requests[0].endpoint_https == "https://example.test/hook");
    assert(transport.requests[0].json_body == json);
    assert(transport.requests[0].bearer_token == "secret-token");
    assert(transport.requests[0].timeout_ms == 3210);
    assert(worker.FailedCount() == 0);

    FakeTransport invalid_transport;
    pdw::AsyncIntegrationWorker invalid_worker(
        invalid_transport, options, "http://example.test/hook",
        pdw::AsyncIntegrationWorker::CredentialProvider());
    assert(!invalid_worker.Start());
    assert(!invalid_worker.TryEnqueue("{}"));

    return 0;
}
