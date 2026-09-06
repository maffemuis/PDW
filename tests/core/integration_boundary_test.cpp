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

class SlowTransport : public pdw::IWebhookTransport
{
public:
    explicit SlowTransport(unsigned long delay_ms) : delay_ms_(delay_ms), calls_(0) {}

    bool PostJson(const pdw::WebhookDeliveryRequest&) override
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++calls_;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms_));
        return true;
    }

    int Calls() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return calls_;
    }

private:
    unsigned long delay_ms_;
    mutable std::mutex mutex_;
    int calls_;
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

bool WaitForFailed(pdw::AsyncIntegrationWorker& worker, std::size_t count)
{
    for (int i = 0; i < 100; ++i)
    {
        if (worker.FailedCount() >= count) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

bool WaitForTransportCalls(const SlowTransport& transport, int count)
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
    options.max_payload_bytes = 1024;
    options.max_outstanding_bytes = 4096;
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
    assert(worker.OutstandingBytes() == 0);
    assert(worker.FailedCount() == 0);

    FakeTransport missing_credential_transport;
    int missing_credential_reads = 0;
    pdw::AsyncIntegrationWorker missing_credential_worker(
        missing_credential_transport,
        options,
        "https://example.test/hook",
        [&missing_credential_reads]() {
            ++missing_credential_reads;
            return std::string();
        });
    assert(missing_credential_worker.Start());
    assert(missing_credential_worker.TryEnqueue(json));
    assert(WaitForFailed(missing_credential_worker, 1));
    missing_credential_worker.Stop();
    assert(missing_credential_reads == 1);
    assert(missing_credential_transport.calls == 0);
    assert(missing_credential_worker.OutstandingBytes() == 0);
    assert(missing_credential_worker.DeliveredCount() == 0);

    FakeTransport oversized_payload_transport;
    pdw::IntegrationWorkerOptions payload_options = options;
    payload_options.max_payload_bytes = 4;
    pdw::AsyncIntegrationWorker oversized_payload_worker(
        oversized_payload_transport, payload_options, "https://example.test/hook",
        pdw::AsyncIntegrationWorker::CredentialProvider());
    assert(oversized_payload_worker.Start());
    assert(oversized_payload_worker.TryEnqueue("1234"));
    assert(!oversized_payload_worker.TryEnqueue("12345"));
    oversized_payload_worker.Stop();
    assert(oversized_payload_worker.DroppedCount() == 1);
    assert(oversized_payload_transport.calls == 1);

    SlowTransport outstanding_transport(100);
    pdw::IntegrationWorkerOptions outstanding_options = options;
    outstanding_options.queue_capacity = 4;
    outstanding_options.max_payload_bytes = 4;
    outstanding_options.max_outstanding_bytes = 4;
    pdw::AsyncIntegrationWorker outstanding_worker(
        outstanding_transport, outstanding_options, "https://example.test/hook",
        pdw::AsyncIntegrationWorker::CredentialProvider());
    assert(outstanding_worker.Start());
    assert(outstanding_worker.TryEnqueue("1234"));
    assert(outstanding_worker.OutstandingBytes() == 4);
    assert(!outstanding_worker.TryEnqueue("1"));
    assert(outstanding_worker.DroppedCount() == 1);
    assert(WaitForDelivered(outstanding_worker, 1));
    assert(outstanding_worker.OutstandingBytes() == 0);
    outstanding_worker.Stop();
    assert(outstanding_transport.Calls() == 1);

    SlowTransport stop_transport(100);
    pdw::IntegrationWorkerOptions stop_options = options;
    stop_options.queue_capacity = 4;
    stop_options.max_payload_bytes = 16;
    stop_options.max_outstanding_bytes = 64;
    stop_options.max_attempts = 1;
    pdw::AsyncIntegrationWorker stop_worker(
        stop_transport, stop_options, "https://example.test/hook",
        pdw::AsyncIntegrationWorker::CredentialProvider());
    assert(stop_worker.Start());
    assert(stop_worker.TryEnqueue("one"));
    assert(stop_worker.TryEnqueue("two"));
    assert(stop_worker.TryEnqueue("three"));
    assert(WaitForTransportCalls(stop_transport, 1));
    stop_worker.Stop();
    assert(stop_transport.Calls() == 1);
    assert(stop_worker.DroppedCount() == 2);
    assert(stop_worker.DeliveredCount() == 1);
    assert(stop_worker.QueueSize() == 0);
    assert(stop_worker.OutstandingBytes() == 0);

    FakeTransport zero_payload_limit_transport;
    pdw::IntegrationWorkerOptions zero_payload_options = options;
    zero_payload_options.max_payload_bytes = 0;
    pdw::AsyncIntegrationWorker zero_payload_worker(
        zero_payload_limit_transport, zero_payload_options, "https://example.test/hook",
        pdw::AsyncIntegrationWorker::CredentialProvider());
    assert(!zero_payload_worker.Start());

    FakeTransport zero_outstanding_limit_transport;
    pdw::IntegrationWorkerOptions zero_outstanding_options = options;
    zero_outstanding_options.max_outstanding_bytes = 0;
    pdw::AsyncIntegrationWorker zero_outstanding_worker(
        zero_outstanding_limit_transport, zero_outstanding_options, "https://example.test/hook",
        pdw::AsyncIntegrationWorker::CredentialProvider());
    assert(!zero_outstanding_worker.Start());

    FakeTransport impossible_limits_transport;
    pdw::IntegrationWorkerOptions impossible_limits_options = options;
    impossible_limits_options.max_payload_bytes = 5;
    impossible_limits_options.max_outstanding_bytes = 4;
    pdw::AsyncIntegrationWorker impossible_limits_worker(
        impossible_limits_transport, impossible_limits_options, "https://example.test/hook",
        pdw::AsyncIntegrationWorker::CredentialProvider());
    assert(!impossible_limits_worker.Start());

    FakeTransport zero_timeout_transport;
    pdw::IntegrationWorkerOptions zero_timeout_options = options;
    zero_timeout_options.request_timeout_ms = 0;
    pdw::AsyncIntegrationWorker zero_timeout_worker(
        zero_timeout_transport, zero_timeout_options, "https://example.test/hook",
        pdw::AsyncIntegrationWorker::CredentialProvider());
    assert(!zero_timeout_worker.Start());

    FakeTransport excessive_timeout_transport;
    pdw::IntegrationWorkerOptions excessive_timeout_options = options;
    excessive_timeout_options.request_timeout_ms = 120001UL;
    pdw::AsyncIntegrationWorker excessive_timeout_worker(
        excessive_timeout_transport, excessive_timeout_options, "https://example.test/hook",
        pdw::AsyncIntegrationWorker::CredentialProvider());
    assert(!excessive_timeout_worker.Start());

    FakeTransport invalid_backoff_transport;
    pdw::IntegrationWorkerOptions invalid_backoff_options = options;
    invalid_backoff_options.initial_backoff_ms = 3;
    invalid_backoff_options.max_backoff_ms = 2;
    pdw::AsyncIntegrationWorker invalid_backoff_worker(
        invalid_backoff_transport, invalid_backoff_options, "https://example.test/hook",
        pdw::AsyncIntegrationWorker::CredentialProvider());
    assert(!invalid_backoff_worker.Start());

    FakeTransport invalid_transport;
    pdw::AsyncIntegrationWorker invalid_worker(
        invalid_transport, options, "http://example.test/hook",
        pdw::AsyncIntegrationWorker::CredentialProvider());
    assert(!invalid_worker.Start());
    assert(!invalid_worker.TryEnqueue("{}"));

    return 0;
}
