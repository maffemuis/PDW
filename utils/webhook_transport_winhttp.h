#ifndef PDW_UTILS_WEBHOOK_TRANSPORT_WINHTTP_H
#define PDW_UTILS_WEBHOOK_TRANSPORT_WINHTTP_H

#include <string>

#include "../core/integration_worker.h"

namespace pdw
{

class WinHttpWebhookTransport : public IWebhookTransport
{
public:
    bool PostJson(const WebhookDeliveryRequest& request) override;

    static bool IsSafeBearerToken(const std::string& token);
};

// Reads a Windows Credential Manager GENERIC credential by target name.
// Secrets are returned only at delivery time and are never persisted by PDW.
std::string ReadWindowsGenericCredentialUtf8(const std::wstring& target_name);

} // namespace pdw

#endif
