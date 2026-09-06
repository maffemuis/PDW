#include "integration_worker.h"

#include <algorithm>
#include <chrono>

namespace pdw
{
namespace
{
const std::size_t kMaxQueueCapacity = 4096;
const std::size_t kMaxPayloadBytes = 8 * 1024 * 1024;
const std::size_t kMaxOutstandingBytes = 64 * 1024 * 1024;
const unsigned int kMaxAttempts = 10;
const unsigned long kMaxBackoffMs = 60000UL;
}

IntegrationWorkerOptions::IntegrationWorkerOptions()
    : queue_capacity(256),
      max_payload_bytes(1024 * 1024),
      max_outstanding_bytes(8 * 1024 * 1024),
      max_attempts(3),
      request_timeout_ms(5000),
      initial_backoff_ms(250),
      max_backoff_ms(4000)
{
}

AsyncIntegrationWorker::AsyncIntegrationWorker(IWebhookTransport& transport,
                                               const IntegrationWorkerOptions& options,
                                               const std::string& endpoint_https,
                                               const CredentialProvider& credential_provider)
    : transport_(transport),
      options_(options),
      endpoint_https_(endpoint_https),
      credential_provider_(credential_provider),
      running_(false),
      stopping_(false),
      outstanding_bytes_(0),
      dropped_(0),
      delivered_(0),
      failed_(0)
{
}

AsyncIntegrationWorker::~AsyncIntegrationWorker()
{
    Stop();
}

bool AsyncIntegrationWorker::IsSafeHttpsEndpoint(const std::string& endpoint)
{
    const std::string prefix("https://");
    if (endpoint.size() <= prefix.size()) return false;
    if (endpoint.compare(0, prefix.size(), prefix) != 0) return false;
    if (endpoint.find_first_of("\r\n\t ") != std::string::npos) return false;
    return true;
}

bool AsyncIntegrationWorker::Start()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) return true;
    if (!IsSafeHttpsEndpoint(endpoint_https_)) return false;
    if (options_.queue_capacity == 0 || options_.queue_capacity > kMaxQueueCapacity) return false;
    if (options_.max_payload_bytes == 0 || options_.max_payload_bytes > kMaxPayloadBytes) return false;
    if (options_.max_outstanding_bytes == 0 || options_.max_outstanding_bytes > kMaxOutstandingBytes) return false;
    if (options_.max_attempts == 0 || options_.max_attempts > kMaxAttempts) return false;
    if (options_.max_payload_bytes > options_.max_outstanding_bytes) return false;
    if (options_.request_timeout_ms == 0 || options_.request_timeout_ms > 120000UL) return false;
    if (options_.initial_backoff_ms > options_.max_backoff_ms) return false;
    if (options_.initial_backoff_ms > kMaxBackoffMs || options_.max_backoff_ms > kMaxBackoffMs) return false;

    stopping_ = false;
    outstanding_bytes_ = 0;
    running_ = true;
    thread_ = std::thread(&AsyncIntegrationWorker::Run, this);
    return true;
}

void AsyncIntegrationWorker::Stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return;
        stopping_ = true;

        while (!queue_.empty())
        {
            outstanding_bytes_ -= queue_.front().size();
            queue_.pop_front();
            ++dropped_;
        }
    }
    wake_.notify_all();
    if (thread_.joinable()) thread_.join();

    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
}

bool AsyncIntegrationWorker::TryEnqueue(const std::string& json_body)
{
    if (json_body.empty()) return false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ || stopping_)
        {
            ++dropped_;
            return false;
        }
        if (json_body.size() > options_.max_payload_bytes)
        {
            ++dropped_;
            return false;
        }
        if (queue_.size() >= options_.queue_capacity)
        {
            ++dropped_;
            return false;
        }
        if (outstanding_bytes_ > options_.max_outstanding_bytes - json_body.size())
        {
            ++dropped_;
            return false;
        }
        queue_.push_back(json_body);
        outstanding_bytes_ += json_body.size();
    }
    wake_.notify_one();
    return true;
}

std::size_t AsyncIntegrationWorker::QueueSize() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

std::size_t AsyncIntegrationWorker::OutstandingBytes() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return outstanding_bytes_;
}

std::size_t AsyncIntegrationWorker::DroppedCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return dropped_;
}

std::size_t AsyncIntegrationWorker::DeliveredCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return delivered_;
}

std::size_t AsyncIntegrationWorker::FailedCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return failed_;
}

void AsyncIntegrationWorker::Run()
{
    for (;;)
    {
        std::string body;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this]() { return stopping_ || !queue_.empty(); });
            if (stopping_ && queue_.empty()) break;
            body = queue_.front();
            queue_.pop_front();
        }

        const bool delivered = DeliverWithRetry(body);
        std::lock_guard<std::mutex> lock(mutex_);
        outstanding_bytes_ -= body.size();
        if (delivered) ++delivered_;
        else ++failed_;
    }
}

bool AsyncIntegrationWorker::DeliverWithRetry(const std::string& json_body)
{
    unsigned long backoff_ms = options_.initial_backoff_ms;
    for (unsigned int attempt = 0; attempt < options_.max_attempts; ++attempt)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) return false;
        }

        WebhookDeliveryRequest request;
        request.endpoint_https = endpoint_https_;
        request.json_body = json_body;
        request.timeout_ms = options_.request_timeout_ms;
        if (credential_provider_)
        {
            try
            {
                request.bearer_token = credential_provider_();
            }
            catch (...)
            {
                return false;
            }
            if (request.bearer_token.empty()) return false;
            if (request.bearer_token.find_first_of("\r\n") != std::string::npos) return false;
        }

        bool posted = false;
        try
        {
            posted = transport_.PostJson(request);
        }
        catch (...)
        {
            posted = false;
        }

        if (posted) return true;
        if (attempt + 1 >= options_.max_attempts) return false;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stopping_) return false;
            if (backoff_ms > 0 &&
                wake_.wait_for(lock, std::chrono::milliseconds(backoff_ms),
                               [this]() { return stopping_; }))
            {
                return false;
            }
        }

        const unsigned long doubled = backoff_ms > (options_.max_backoff_ms / 2)
            ? options_.max_backoff_ms
            : backoff_ms * 2;
        backoff_ms = std::min(doubled, options_.max_backoff_ms);
    }
    return false;
}

} // namespace pdw
