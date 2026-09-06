#include "webhook_transport_winhttp.h"

#include <windows.h>
#include <wincred.h>
#include <winhttp.h>

#include <limits>
#include <vector>

namespace pdw
{
namespace
{
class WinHttpHandle
{
public:
    explicit WinHttpHandle(HINTERNET handle = NULL) : handle_(handle) {}
    ~WinHttpHandle() { if (handle_) WinHttpCloseHandle(handle_); }
    HINTERNET get() const { return handle_; }
    operator bool() const { return handle_ != NULL; }
private:
    WinHttpHandle(const WinHttpHandle&);
    WinHttpHandle& operator=(const WinHttpHandle&);
    HINTERNET handle_;
};

bool Utf8ToWide(const std::string& input, std::wstring& output)
{
    output.clear();
    if (input.empty()) return true;
    if (input.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return false;
    const int input_size = static_cast<int>(input.size());
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          input.data(), input_size,
                                          NULL, 0);
    if (count <= 0) return false;
    output.resize(static_cast<std::size_t>(count));
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                               input.data(), input_size,
                               &output[0], count) == count;
}
}

WebhookRuntimeConfig::WebhookRuntimeConfig()
    : enabled(false),
      request_timeout_ms(5000)
{
}

bool WebhookRuntimeConfig::IsValid() const
{
    if (!enabled) return true;
    if (!AsyncIntegrationWorker::IsSafeHttpsEndpoint(endpoint_https)) return false;
    if (!WinHttpWebhookTransport::IsSafeCredentialTarget(credential_target)) return false;
    if (request_timeout_ms == 0 || request_timeout_ms > 120000UL) return false;
    return true;
}

bool WinHttpWebhookTransport::IsSafeBearerToken(const std::string& token)
{
    return token.find_first_of("\r\n") == std::string::npos;
}

bool WinHttpWebhookTransport::IsSafeCredentialTarget(const std::wstring& target_name)
{
    if (target_name.empty() || target_name.size() > CRED_MAX_GENERIC_TARGET_NAME_LENGTH)
        return false;
    for (std::wstring::const_iterator it = target_name.begin(); it != target_name.end(); ++it)
    {
        if (*it < 0x20 || *it == 0x7f) return false;
    }
    return true;
}

bool WinHttpWebhookTransport::IsSafeBodySize(std::size_t body_size)
{
    const std::size_t kMaxDirectWebhookBodyBytes = 8u * 1024u * 1024u;
    return body_size <= kMaxDirectWebhookBodyBytes;
}

bool WinHttpWebhookTransport::PostJson(const WebhookDeliveryRequest& request)
{
    if (!AsyncIntegrationWorker::IsSafeHttpsEndpoint(request.endpoint_https)) return false;
    if (!IsSafeBearerToken(request.bearer_token)) return false;
    if (!IsSafeBodySize(request.json_body.size())) return false;
    if (request.timeout_ms == 0 || request.timeout_ms > 120000UL) return false;

    std::wstring endpoint;
    if (!Utf8ToWide(request.endpoint_https, endpoint)) return false;

    URL_COMPONENTS parts = {};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(endpoint.c_str(), 0, 0, &parts)) return false;
    if (parts.nScheme != INTERNET_SCHEME_HTTPS || parts.dwHostNameLength == 0) return false;

    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring path;
    if (parts.dwUrlPathLength > 0) path.assign(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (path.empty()) path = L"/";
    if (parts.dwExtraInfoLength > 0) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);

    WinHttpHandle session(WinHttpOpen(L"PDW/3 webhook",
                                      WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) return false;

    const int timeout = static_cast<int>(request.timeout_ms);
    if (!WinHttpSetTimeouts(session.get(), timeout, timeout, timeout, timeout)) return false;

    WinHttpHandle connection(WinHttpConnect(session.get(), host.c_str(), parts.nPort, 0));
    if (!connection) return false;

    WinHttpHandle http_request(WinHttpOpenRequest(connection.get(), L"POST", path.c_str(),
                                                  NULL, WINHTTP_NO_REFERER,
                                                  WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                  WINHTTP_FLAG_SECURE));
    if (!http_request) return false;

    std::wstring headers = L"Content-Type: application/json; charset=utf-8\r\nAccept: application/json\r\n";
    if (!request.bearer_token.empty())
    {
        std::wstring token;
        if (!Utf8ToWide(request.bearer_token, token)) return false;
        headers += L"Authorization: Bearer ";
        headers += token;
        headers += L"\r\n";
    }

    const DWORD body_size = static_cast<DWORD>(request.json_body.size());
    if (!WinHttpSendRequest(http_request.get(), headers.c_str(), static_cast<DWORD>(-1),
                            request.json_body.empty() ? WINHTTP_NO_REQUEST_DATA :
                                const_cast<char*>(request.json_body.data()),
                            body_size, body_size, 0)) return false;
    if (!WinHttpReceiveResponse(http_request.get(), NULL)) return false;

    DWORD status = 0;
    DWORD status_size = sizeof(status);
    if (!WinHttpQueryHeaders(http_request.get(),
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX,
                             &status, &status_size, WINHTTP_NO_HEADER_INDEX)) return false;
    return status >= 200 && status < 300;
}

std::string ReadWindowsGenericCredentialUtf8(const std::wstring& target_name)
{
    if (!WinHttpWebhookTransport::IsSafeCredentialTarget(target_name)) return std::string();

    PCREDENTIALW credential = NULL;
    if (!CredReadW(target_name.c_str(), CRED_TYPE_GENERIC, 0, &credential))
        return std::string();

    std::string result;
    if (credential->CredentialBlob && credential->CredentialBlobSize > 0)
    {
        const char* bytes = reinterpret_cast<const char*>(credential->CredentialBlob);
        result.assign(bytes, bytes + credential->CredentialBlobSize);
        while (!result.empty() && result.back() == '\0') result.pop_back();
    }
    CredFree(credential);
    return result;
}

} // namespace pdw
