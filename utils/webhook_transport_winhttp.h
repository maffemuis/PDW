#ifndef PDW_UTILS_WEBHOOK_TRANSPORT_WINHTTP_H
#define PDW_UTILS_WEBHOOK_TRANSPORT_WINHTTP_H

#include <string>

#include "../core/integration_worker.h"

namespace pdw
{

struct WebhookRuntimeConfig
{
    WebhookRuntimeConfig();

    bool enabled;
    std::string endpoint_https;
    std::wstring credential_target;
    unsigned long request_timeout_ms;

    // Secrets are intentionally not part of this structure. Only the
    // Credential Manager target name may be persisted by a future UI layer.
    bool IsValid() const;
};

class WinHttpWebhookTransport : public IWebhookTransport
{
public:
    bool PostJson(const WebhookDeliveryRequest& request) override;

    static bool IsSafeBearerToken(const std::string& token);
    static bool IsSafeCredentialTarget(const std::wstring& target_name);
};

// Reads a Windows Credential Manager GENERIC credential by target name.
// Secrets are returned only at delivery time and are never persisted by PDW.
std::string ReadWindowsGenericCredentialUtf8(const std::wstring& target_name);

} // namespace pdw

#endif
