#ifndef PDW_CORE_INTEGRATION_WORKER_H
#define PDW_CORE_INTEGRATION_WORKER_H

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace pdw
{

struct WebhookDeliveryRequest
{
    std::string endpoint_https;
    std::string json_body;
    std::string bearer_token;
    unsigned long timeout_ms;
};

class IWebhookTransport
{
public:
    virtual ~IWebhookTransport() {}
    virtual bool PostJson(const WebhookDeliveryRequest& request) = 0;
};

struct IntegrationWorkerOptions
{
    IntegrationWorkerOptions();

    std::size_t queue_capacity;
    std::size_t max_payload_bytes;
    std::size_t max_outstanding_bytes;
    unsigned int max_attempts;
    unsigned long request_timeout_ms;
    unsigned long initial_backoff_ms;
    unsigned long max_backoff_ms;
};

class AsyncIntegrationWorker
{
public:
    typedef std::function<std::string(void)> CredentialProvider;

    AsyncIntegrationWorker(IWebhookTransport& transport,
                           const IntegrationWorkerOptions& options,
                           const std::string& endpoint_https,
                           const CredentialProvider& credential_provider);
    ~AsyncIntegrationWorker();

    bool Start();
    void Stop();

    // Fail-open contract: returns false when disabled/full/invalid, but never
    // blocks decoder callers on network I/O.
    bool TryEnqueue(const std::string& json_body);

    std::size_t QueueSize() const;
    std::size_t OutstandingBytes() const;
    std::size_t DroppedCount() const;
    std::size_t DeliveredCount() const;
    std::size_t FailedCount() const;

    static bool IsSafeHttpsEndpoint(const std::string& endpoint);

private:
    AsyncIntegrationWorker(const AsyncIntegrationWorker&);
    AsyncIntegrationWorker& operator=(const AsyncIntegrationWorker&);

    void Run();
    bool DeliverWithRetry(const std::string& json_body);

    IWebhookTransport& transport_;
    IntegrationWorkerOptions options_;
    std::string endpoint_https_;
    CredentialProvider credential_provider_;

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::string> queue_;
    std::thread thread_;
    bool running_;
    bool stopping_;
    std::size_t outstanding_bytes_;
    std::size_t dropped_;
    std::size_t delivered_;
    std::size_t failed_;
};

} // namespace pdw

#endif
