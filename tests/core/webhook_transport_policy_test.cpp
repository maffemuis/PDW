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

    // Missing credentials fail closed for authentication but do not touch
    // decoder state: caller receives an empty token and worker delivery may fail.
    assert(pdw::ReadWindowsGenericCredentialUtf8(L"PDW-test-credential-that-must-not-exist-6df5a0")
               .empty());
    return 0;
}
