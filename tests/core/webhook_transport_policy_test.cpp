#include <cassert>
#include <string>

#include "integration_worker.h"
#include "webhook_transport_winhttp.h"

int main()
{
    assert(pdw::AsyncIntegrationWorker::IsSafeHttpsEndpoint("https://example.test/hook"));
    assert(!pdw::AsyncIntegrationWorker::IsSafeHttpsEndpoint("http://example.test/hook"));

    assert(pdw::WinHttpWebhookTransport::IsSafeBearerToken("token-value"));
    assert(pdw::WinHttpWebhookTransport::IsSafeBearerToken(""));
    assert(!pdw::WinHttpWebhookTransport::IsSafeBearerToken("abc\r\nX-Evil: yes"));
    assert(!pdw::WinHttpWebhookTransport::IsSafeBearerToken("abc\ndef"));

    assert(pdw::WinHttpWebhookTransport::IsSafeCredentialTarget(L"PDW/Webhook/default"));
    assert(!pdw::WinHttpWebhookTransport::IsSafeCredentialTarget(L""));
    assert(!pdw::WinHttpWebhookTransport::IsSafeCredentialTarget(L"PDW/Webhook\nInjected"));

    const std::size_t max_body = 8u * 1024u * 1024u;
    assert(pdw::WinHttpWebhookTransport::IsSafeBodySize(0));
    assert(pdw::WinHttpWebhookTransport::IsSafeBodySize(max_body));
    assert(!pdw::WinHttpWebhookTransport::IsSafeBodySize(max_body + 1));

    pdw::WebhookRuntimeConfig disabled;
    assert(disabled.IsValid());

    pdw::WebhookRuntimeConfig valid;
    valid.enabled = true;
    valid.endpoint_https = "https://example.test/pdw";
    valid.credential_target = L"PDW/Webhook/default";
    valid.request_timeout_ms = 5000;
    assert(valid.IsValid());

    pdw::WebhookRuntimeConfig insecure = valid;
    insecure.endpoint_https = "http://example.test/pdw";
    assert(!insecure.IsValid());

    pdw::WebhookRuntimeConfig no_credential_target = valid;
    no_credential_target.credential_target.clear();
    assert(!no_credential_target.IsValid());

    pdw::WebhookRuntimeConfig bad_timeout = valid;
    bad_timeout.request_timeout_ms = 0;
    assert(!bad_timeout.IsValid());

    // Missing credentials fail closed for authentication but do not touch
    // decoder state: caller receives an empty token and worker delivery may fail.
    assert(pdw::ReadWindowsGenericCredentialUtf8(L"PDW-test-credential-that-must-not-exist-6df5a0")
               .empty());
    return 0;
}
