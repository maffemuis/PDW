#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wincrypt.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

#include "smtp.h"
#include "smtp_protocol.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Crypt32.lib")

extern int nSMTPerrors;
extern int iSMTPlastError;
extern int nSMTPemails;
extern int nSMTPsessions;

namespace
{
const int kQueueSize = 100;
const size_t kQueueMessageSize = 8192;
const DWORD kSocketTimeoutMs = 30000;

enum ModernSmtpError
{
    SMTP_MODERN_CONFIG = 600,
    SMTP_MODERN_CONNECT,
    SMTP_MODERN_PROTOCOL,
    SMTP_MODERN_TLS,
    SMTP_MODERN_AUTH,
    SMTP_MODERN_SEND,
    SMTP_MODERN_QUEUE
};

struct MailConfig
{
    std::string host;
    std::string helo;
    std::string from;
    std::string to;
    std::string user;
    std::string credential;
    int port = 0;
    int options = 0;
};

struct Transport
{
    SOCKET socket = INVALID_SOCKET;
    SSL_CTX *ctx = NULL;
    SSL *ssl = NULL;
    bool tls_active = false;
    std::string host;
};

CRITICAL_SECTION g_lock;
bool g_lock_ready = false;
HANDLE g_mail_thread = NULL;
volatile LONG g_keep_running = 0;
MailConfig g_config;
HWND g_response = NULL;
char g_queue[kQueueSize][kQueueMessageSize];
int g_queue_start = 0;
int g_queue_end = 0;

void EnsureLock()
{
    if (!g_lock_ready)
    {
        InitializeCriticalSection(&g_lock);
        g_lock_ready = true;
    }
}

void AddResponse(const std::string &text)
{
    HWND response = NULL;
    EnsureLock();
    EnterCriticalSection(&g_lock);
    response = g_response;
    LeaveCriticalSection(&g_lock);

    if (response)
    {
        SendMessageA(response, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
    }

    OutputDebugStringA(text.c_str());
}

void RecordError(int code, const char *message)
{
    ++nSMTPerrors;
    iSMTPlastError = code;
    if (message)
    {
        AddResponse(std::string("SMTP error: ") + message + "\n");
    }
}

void SecureClear(std::string &value)
{
    if (!value.empty())
    {
        SecureZeroMemory(&value[0], value.size());
        value.clear();
    }
}

bool InitWinsock()
{
    static bool initialized = false;
    if (initialized)
    {
        return true;
    }

    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
    {
        return false;
    }

    initialized = true;
    return true;
}

SOCKET ConnectSocket(const std::string &host, int port)
{
    if (!InitWinsock())
    {
        return INVALID_SOCKET;
    }

    char service[16];
    _snprintf(service, sizeof(service) - 1, "%d", port);
    service[sizeof(service) - 1] = '\0';

    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo *addresses = NULL;
    if (getaddrinfo(host.c_str(), service, &hints, &addresses) != 0)
    {
        return INVALID_SOCKET;
    }

    SOCKET connected = INVALID_SOCKET;
    for (addrinfo *address = addresses; address; address = address->ai_next)
    {
        SOCKET candidate = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (candidate == INVALID_SOCKET)
        {
            continue;
        }

        DWORD timeout = kSocketTimeoutMs;
        setsockopt(candidate, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
        setsockopt(candidate, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));

        if (connect(candidate, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0)
        {
            connected = candidate;
            break;
        }

        closesocket(candidate);
    }

    freeaddrinfo(addresses);
    return connected;
}

bool LoadWindowsRootCertificates(SSL_CTX *ctx)
{
    HCERTSTORE root_store = CertOpenSystemStoreA(0, "ROOT");
    if (!root_store)
    {
        return false;
    }

    X509_STORE *openssl_store = SSL_CTX_get_cert_store(ctx);
    PCCERT_CONTEXT certificate = NULL;
    int imported = 0;

    while ((certificate = CertEnumCertificatesInStore(root_store, certificate)) != NULL)
    {
        const unsigned char *encoded = certificate->pbCertEncoded;
        X509 *x509 = d2i_X509(NULL, &encoded, certificate->cbCertEncoded);
        if (!x509)
        {
            ERR_clear_error();
            continue;
        }

        if (X509_STORE_add_cert(openssl_store, x509) == 1)
        {
            ++imported;
        }
        else
        {
            // Duplicate roots are harmless when OpenSSL's default paths also
            // contain a certificate from the Windows trust store.
            ERR_clear_error();
        }
        X509_free(x509);
    }

    CertCloseStore(root_store, 0);
    return imported > 0;
}

bool EnableTls(Transport &transport)
{
    transport.ctx = SSL_CTX_new(TLS_client_method());
    if (!transport.ctx)
    {
        return false;
    }

    if (SSL_CTX_set_min_proto_version(transport.ctx, TLS1_2_VERSION) != 1)
    {
        return false;
    }

    SSL_CTX_set_options(transport.ctx, SSL_OP_NO_COMPRESSION);
    SSL_CTX_set_verify(transport.ctx, SSL_VERIFY_PEER, NULL);
    SSL_CTX_set_default_verify_paths(transport.ctx);

    if (!LoadWindowsRootCertificates(transport.ctx))
    {
        return false;
    }

    transport.ssl = SSL_new(transport.ctx);
    if (!transport.ssl)
    {
        return false;
    }

    if (SSL_set_tlsext_host_name(transport.ssl, transport.host.c_str()) != 1)
    {
        return false;
    }

    if (SSL_set1_host(transport.ssl, transport.host.c_str()) != 1)
    {
        return false;
    }

    SSL_set_fd(transport.ssl, static_cast<int>(transport.socket));
    if (SSL_connect(transport.ssl) != 1)
    {
        return false;
    }

    if (SSL_get_verify_result(transport.ssl) != X509_V_OK)
    {
        return false;
    }

    transport.tls_active = true;
    return true;
}

void CloseTransport(Transport &transport)
{
    if (transport.ssl)
    {
        SSL_shutdown(transport.ssl);
        SSL_free(transport.ssl);
        transport.ssl = NULL;
    }
    if (transport.ctx)
    {
        SSL_CTX_free(transport.ctx);
        transport.ctx = NULL;
    }
    if (transport.socket != INVALID_SOCKET)
    {
        closesocket(transport.socket);
        transport.socket = INVALID_SOCKET;
    }
    transport.tls_active = false;
}

bool WriteAll(Transport &transport, const char *data, size_t length)
{
    size_t offset = 0;
    while (offset < length)
    {
        int written = 0;
        if (transport.tls_active)
        {
            written = SSL_write(transport.ssl, data + offset, static_cast<int>(length - offset));
            if (written <= 0)
            {
                return false;
            }
        }
        else
        {
            written = send(transport.socket, data + offset, static_cast<int>(length - offset), 0);
            if (written <= 0)
            {
                return false;
            }
        }
        offset += static_cast<size_t>(written);
    }
    return true;
}

bool ReadByte(Transport &transport, char &value)
{
    int received = transport.tls_active
        ? SSL_read(transport.ssl, &value, 1)
        : recv(transport.socket, &value, 1, 0);
    return received == 1;
}

bool ReadLine(Transport &transport, std::string &line)
{
    line.clear();
    char ch = 0;
    while (line.size() < 4096)
    {
        if (!ReadByte(transport, ch))
        {
            return false;
        }
        if (ch == '\n')
        {
            if (!line.empty() && line[line.size() - 1] == '\r')
            {
                line.resize(line.size() - 1);
            }
            return true;
        }
        line.push_back(ch);
    }
    return false;
}

bool ReadResponse(Transport &transport, int &code, std::string *capabilities = NULL)
{
    code = -1;
    if (capabilities)
    {
        capabilities->clear();
    }

    for (int line_count = 0; line_count < 100; ++line_count)
    {
        std::string line;
        if (!ReadLine(transport, line) || line.size() < 3
            || line[0] < '0' || line[0] > '9'
            || line[1] < '0' || line[1] > '9'
            || line[2] < '0' || line[2] > '9')
        {
            return false;
        }

        const int line_code = (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');
        if (code == -1)
        {
            code = line_code;
        }
        else if (line_code != code)
        {
            return false;
        }

        AddResponse(std::string("< ") + line + "\n");

        if (capabilities && line.size() > 4)
        {
            *capabilities += line.substr(4);
            capabilities->push_back('\n');
        }

        if (line.size() < 4 || line[3] != '-')
        {
            return true;
        }
    }

    return false;
}

bool SendCommand(Transport &transport, const std::string &command, const std::string &display)
{
    AddResponse(std::string("> ") + display + "\n");
    return WriteAll(transport, command.data(), command.size());
}

bool ExpectCode(Transport &transport, int expected, std::string *capabilities = NULL)
{
    int code = -1;
    return ReadResponse(transport, code, capabilities) && code == expected;
}

bool SendEhlo(Transport &transport, const std::string &helo, std::string &capabilities)
{
    const std::string command = "EHLO " + helo + "\r\n";
    return SendCommand(transport, command, "EHLO " + helo) && ExpectCode(transport, 250, &capabilities);
}

std::string Trim(const std::string &value)
{
    size_t first = 0;
    while (first < value.size() && (value[first] == ' ' || value[first] == '\t'))
    {
        ++first;
    }

    size_t last = value.size();
    while (last > first && (value[last - 1] == ' ' || value[last - 1] == '\t'))
    {
        --last;
    }
    return value.substr(first, last - first);
}

std::vector<std::string> SplitRecipients(const std::string &input)
{
    std::vector<std::string> recipients;
    size_t start = 0;
    while (start <= input.size())
    {
        const size_t end = input.find_first_of(",;", start);
        const std::string item = Trim(input.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (!item.empty())
        {
            recipients.push_back(item);
        }
        if (end == std::string::npos)
        {
            break;
        }
        start = end + 1;
    }
    return recipients;
}

bool ReadTokenFile(const char *path, std::string &token)
{
    if (!path || !path[0])
    {
        return false;
    }

    FILE *file = fopen(path, "rb");
    if (!file)
    {
        return false;
    }

    char buffer[16384];
    const size_t count = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    buffer[count] = '\0';

    token.assign(buffer, count);
    while (!token.empty() && (token[token.size() - 1] == '\r' || token[token.size() - 1] == '\n' || token[token.size() - 1] == ' ' || token[token.size() - 1] == '\t'))
    {
        token.resize(token.size() - 1);
    }
    return !token.empty();
}

bool GetOAuthToken(const MailConfig &config, std::string &token, bool &oauth_required)
{
    token.clear();
    oauth_required = false;

    const char *mode = getenv("PDW_SMTP_AUTH_MODE");
    if (mode && _stricmp(mode, "oauth2") == 0)
    {
        oauth_required = true;
    }
    else if (mode && _stricmp(mode, "login") == 0)
    {
        return false;
    }

    const char *token_file = getenv("PDW_SMTP_OAUTH2_TOKEN_FILE");
    if (ReadTokenFile(token_file, token))
    {
        return true;
    }

    const char *environment_token = getenv("PDW_SMTP_OAUTH2_TOKEN");
    if (environment_token && environment_token[0])
    {
        token = environment_token;
        return true;
    }

    const char prefix[] = "oauth2:";
    if (config.credential.size() > sizeof(prefix) - 1
        && _strnicmp(config.credential.c_str(), prefix, sizeof(prefix) - 1) == 0)
    {
        token = config.credential.substr(sizeof(prefix) - 1);
        oauth_required = true;
        return !token.empty();
    }

    return false;
}

bool Authenticate(Transport &transport, const MailConfig &config, const std::string &capabilities)
{
    if (!(config.options & MAIL_OPTION_AUTH))
    {
        return true;
    }

    // Never expose any credential on a cleartext SMTP session.
    if (!transport.tls_active)
    {
        RecordError(SMTP_MODERN_AUTH, "authentication requires TLS");
        return false;
    }

    std::string token;
    bool oauth_required = false;
    const bool have_oauth = GetOAuthToken(config, token, oauth_required);

    if (have_oauth || oauth_required)
    {
        if (!have_oauth)
        {
            RecordError(SMTP_MODERN_AUTH, "OAuth2 selected but no access token is available");
            return false;
        }
        if (!pdw::HasSmtpCapability(capabilities, "XOAUTH2"))
        {
            RecordError(SMTP_MODERN_AUTH, "server does not advertise XOAUTH2");
            SecureClear(token);
            return false;
        }

        const std::string response = pdw::BuildXOAuth2InitialResponse(config.user, token);
        SecureClear(token);
        const std::string command = "AUTH XOAUTH2 " + response + "\r\n";
        if (!SendCommand(transport, command, "AUTH XOAUTH2 <redacted>"))
        {
            return false;
        }

        int code = -1;
        if (!ReadResponse(transport, code))
        {
            return false;
        }

        if (code == 235)
        {
            return true;
        }

        // Gmail can return a 334 JSON challenge when the bearer token is
        // rejected. Complete the SASL exchange without echoing the challenge.
        if (code == 334)
        {
            if (!SendCommand(transport, "\r\n", "<empty OAuth2 failure response>"))
            {
                return false;
            }
            ReadResponse(transport, code);
        }

        RecordError(SMTP_MODERN_AUTH, "XOAUTH2 authentication failed");
        return false;
    }

    if (!pdw::HasSmtpCapability(capabilities, "LOGIN"))
    {
        RecordError(SMTP_MODERN_AUTH, "server does not advertise AUTH LOGIN");
        return false;
    }

    if (!SendCommand(transport, "AUTH LOGIN\r\n", "AUTH LOGIN") || !ExpectCode(transport, 334))
    {
        return false;
    }

    const std::string user = pdw::Base64Encode(config.user) + "\r\n";
    if (!SendCommand(transport, user, "<username redacted>") || !ExpectCode(transport, 334))
    {
        return false;
    }

    const std::string password = pdw::Base64Encode(config.credential) + "\r\n";
    if (!SendCommand(transport, password, "<password redacted>") || !ExpectCode(transport, 235))
    {
        RecordError(SMTP_MODERN_AUTH, "AUTH LOGIN failed");
        return false;
    }

    return true;
}

std::string SelectedCharset(int options)
{
    static const char *charsets[] = {
        "us-ascii", "iso-8859-1", "iso-8859-2", "iso-8859-3", "iso-8859-4",
        "iso-8859-5", "iso-8859-6", "iso-8859-7", "iso-8859-8", "iso-8859-9",
        "iso-8859-10", "iso-2022-kr", "KOI8-R", "EUC-KR", "Shift_JIS",
        "ISO-2022-JP", "EUC-JP", "GB2312", "Big5", "windows-1250", "windows-1251",
        "windows-1252", "windows-1253", "windows-1254", "windows-1255", "windows-1256",
        "windows-1257", "windows-1258"
    };

    const int encoded = ((options & 0x1F0000) >> 16);
    const int index = encoded > 0 && encoded <= 28 ? encoded - 1 : 0;
    return charsets[index];
}

void BuildSubjectAndBody(const std::string &payload, std::string &subject, std::string &body)
{
    subject.clear();
    body.clear();
    for (size_t i = 0; i < payload.size(); ++i)
    {
        const unsigned char ch = static_cast<unsigned char>(payload[i]);
        if (ch == 0xBB)
        {
            subject += " - ";
            body += "\r\n";
        }
        else
        {
            subject.push_back(static_cast<char>(ch));
            body.push_back(static_cast<char>(ch));
        }
    }
}

std::string DotStuffAndNormalize(const std::string &text)
{
    std::string normalized;
    normalized.reserve(text.size() + 32);
    bool line_start = true;

    for (size_t i = 0; i < text.size(); ++i)
    {
        char ch = text[i];
        if (ch == '\r')
        {
            if (i + 1 < text.size() && text[i + 1] == '\n')
            {
                ++i;
            }
            normalized += "\r\n";
            line_start = true;
            continue;
        }
        if (ch == '\n')
        {
            normalized += "\r\n";
            line_start = true;
            continue;
        }
        if (line_start && ch == '.')
        {
            normalized.push_back('.');
        }
        normalized.push_back(ch);
        line_start = false;
    }

    if (normalized.size() < 2 || normalized.substr(normalized.size() - 2) != "\r\n")
    {
        normalized += "\r\n";
    }
    return normalized;
}

bool SendEnvelopeAndData(Transport &transport, const MailConfig &config, const std::string &payload)
{
    if (!SendCommand(transport, "MAIL FROM:<" + config.from + ">\r\n", "MAIL FROM:<" + config.from + ">")
        || !ExpectCode(transport, 250))
    {
        return false;
    }

    const std::vector<std::string> recipients = SplitRecipients(config.to);
    if (recipients.empty())
    {
        RecordError(SMTP_MODERN_CONFIG, "no recipient configured");
        return false;
    }

    for (size_t i = 0; i < recipients.size(); ++i)
    {
        if (!SendCommand(transport, "RCPT TO:<" + recipients[i] + ">\r\n", "RCPT TO:<" + recipients[i] + ">"))
        {
            return false;
        }
        int code = -1;
        if (!ReadResponse(transport, code) || (code != 250 && code != 251))
        {
            return false;
        }
    }

    if (!SendCommand(transport, "DATA\r\n", "DATA") || !ExpectCode(transport, 354))
    {
        return false;
    }

    std::string subject;
    std::string body;
    BuildSubjectAndBody(payload, subject, body);

    std::string message;
    if (config.options & MAIL_OPTION_SUBJECT)
    {
        message += "Subject: " + pdw::SanitizeSmtpHeaderValue(subject) + "\r\n";
    }
    message += "From: " + config.from + "\r\n";
    message += "To: " + config.to + "\r\n";
    message += "MIME-Version: 1.0\r\n";
    message += "Content-Type: text/plain; charset=\"" + SelectedCharset(config.options) + "\"\r\n";
    message += "Content-Transfer-Encoding: 8bit\r\n";
    message += "X-Mailer: PDW Modern SMTP\r\n";
    message += "\r\n";
    if (config.options & MAIL_OPTION_MSG)
    {
        message += body;
    }

    message = DotStuffAndNormalize(message);
    if (!WriteAll(transport, message.data(), message.size()) || !WriteAll(transport, ".\r\n", 3))
    {
        return false;
    }

    return ExpectCode(transport, 250);
}

bool SendSmtpMessage(const MailConfig &config, const std::string &payload)
{
    if (config.host.empty() || config.from.empty() || config.to.empty() || config.port <= 0)
    {
        RecordError(SMTP_MODERN_CONFIG, "host, port, from and to are required");
        return false;
    }

    Transport transport;
    transport.host = config.host;
    transport.socket = ConnectSocket(config.host, config.port);
    if (transport.socket == INVALID_SOCKET)
    {
        RecordError(SMTP_MODERN_CONNECT, "connection failed");
        return false;
    }
    ++nSMTPsessions;

    const pdw::SmtpTlsMode tls_mode = pdw::SelectSmtpTlsMode((config.options & MAIL_OPTION_SSL) != 0, config.port);

    if (tls_mode == pdw::SmtpTlsMode::ImplicitTls && !EnableTls(transport))
    {
        RecordError(SMTP_MODERN_TLS, "implicit TLS handshake or certificate validation failed");
        CloseTransport(transport);
        return false;
    }

    if (!ExpectCode(transport, 220))
    {
        RecordError(SMTP_MODERN_PROTOCOL, "invalid SMTP greeting");
        CloseTransport(transport);
        return false;
    }

    const std::string helo = config.helo.empty() ? "localhost" : config.helo;
    std::string capabilities;
    if (!SendEhlo(transport, helo, capabilities))
    {
        RecordError(SMTP_MODERN_PROTOCOL, "EHLO failed");
        CloseTransport(transport);
        return false;
    }

    if (tls_mode == pdw::SmtpTlsMode::StartTls)
    {
        if (!pdw::HasSmtpCapability(capabilities, "STARTTLS"))
        {
            RecordError(SMTP_MODERN_TLS, "TLS was required but server did not advertise STARTTLS");
            CloseTransport(transport);
            return false;
        }

        if (!SendCommand(transport, "STARTTLS\r\n", "STARTTLS") || !ExpectCode(transport, 220)
            || !EnableTls(transport))
        {
            RecordError(SMTP_MODERN_TLS, "STARTTLS handshake or certificate validation failed");
            CloseTransport(transport);
            return false;
        }

        if (!SendEhlo(transport, helo, capabilities))
        {
            RecordError(SMTP_MODERN_PROTOCOL, "EHLO after STARTTLS failed");
            CloseTransport(transport);
            return false;
        }
    }

    if (!Authenticate(transport, config, capabilities))
    {
        CloseTransport(transport);
        return false;
    }

    if (!SendEnvelopeAndData(transport, config, payload))
    {
        RecordError(SMTP_MODERN_SEND, "message transaction failed");
        CloseTransport(transport);
        return false;
    }

    ++nSMTPemails;
    SendCommand(transport, "QUIT\r\n", "QUIT");
    int ignored = -1;
    ReadResponse(transport, ignored);
    CloseTransport(transport);
    return true;
}

bool PopQueued(std::string &payload, MailConfig &config)
{
    EnsureLock();
    EnterCriticalSection(&g_lock);
    if (g_queue_start == g_queue_end)
    {
        LeaveCriticalSection(&g_lock);
        return false;
    }

    payload = g_queue[g_queue_end];
    g_queue_end = (g_queue_end + 1) % kQueueSize;
    config = g_config;
    LeaveCriticalSection(&g_lock);
    return true;
}

DWORD WINAPI MailThreadFunc(LPVOID)
{
    while (InterlockedCompareExchange(&g_keep_running, 1, 1) == 1)
    {
        std::string payload;
        MailConfig config;
        if (PopQueued(payload, config))
        {
            SendSmtpMessage(config, payload);
            SecureClear(config.credential);
        }
        else
        {
            Sleep(250);
        }
    }
    return 0;
}

void StartThreadIfNeeded()
{
    if (g_mail_thread)
    {
        return;
    }

    InterlockedExchange(&g_keep_running, 1);
    DWORD thread_id = 0;
    g_mail_thread = CreateThread(NULL, 0, MailThreadFunc, NULL, 0, &thread_id);
    if (!g_mail_thread)
    {
        InterlockedExchange(&g_keep_running, 0);
        RecordError(SMTP_MODERN_CONFIG, "could not start mail worker");
    }
}

void StopThread()
{
    if (!g_mail_thread)
    {
        return;
    }

    InterlockedExchange(&g_keep_running, 0);
    WaitForSingleObject(g_mail_thread, 35000);
    CloseHandle(g_mail_thread);
    g_mail_thread = NULL;
}

bool QueuePayload(const std::string &payload)
{
    EnsureLock();
    EnterCriticalSection(&g_lock);
    const int next = (g_queue_start + 1) % kQueueSize;
    if (next == g_queue_end || payload.size() >= kQueueMessageSize)
    {
        LeaveCriticalSection(&g_lock);
        RecordError(SMTP_MODERN_QUEUE, "mail queue is full or message is too large");
        return false;
    }

    memcpy(g_queue[g_queue_start], payload.c_str(), payload.size() + 1);
    g_queue_start = next;
    LeaveCriticalSection(&g_lock);
    return true;
}

int CurrentOptions()
{
    EnsureLock();
    EnterCriticalSection(&g_lock);
    const int options = g_config.options;
    LeaveCriticalSection(&g_lock);
    return options;
}
}

char *szSmtpCharSets[] = {
    "us-ascii     (Standard)", "iso-8859-1   (West European)", "iso-8859-2   (East European)",
    "iso-8859-3   (South European)", "iso-8859-4   (North European)", "iso-8859-5   (Cyrillic)",
    "iso-8859-6   (Arabic)", "iso-8859-7   (Greek)", "iso-8859-8   (Hebrew)",
    "iso-8859-9   (Turkish)", "iso-8859-10  (Nordic)", "iso-2022-kr  (Korean)",
    "KOI8-R       (Russian)", "EUC-KR       (Korean)", "Shift_JIS    (Japanese)",
    "ISO-2022-JP  (Japanese)", "EUC-JP       (Japanese)", "GB2312       (Chinese)",
    "Big5         (Traditional Chinese)", "windows-1250 (Central Europ Windows)",
    "windows-1251 (Cyrillic Windows)", "windows-1252 (Western Europ Windows)",
    "windows-1253 (Greek Windows)", "windows-1254 (Turkish Windows)",
    "windows-1255 (Hebrew Windows)", "windows-1256 (Arabic Windows)",
    "windows-1257 (Baltic Windows)", "windows-1258 (Vietnamese Windows)"
};

int MailInit(char *szMailHost, char *szMailHeloDomain, char *szMailFrom, char *szMailTo,
    char *szMailUser, char *szMailPassword, int iMailPort, int nOptions)
{
    EnsureLock();

    if (!(nOptions & MAIL_OPTION_ENABLE))
    {
        StopThread();
    }

    EnterCriticalSection(&g_lock);
    SecureClear(g_config.credential);
    g_config.host = szMailHost ? szMailHost : "";
    g_config.helo = szMailHeloDomain ? szMailHeloDomain : "";
    g_config.from = szMailFrom ? szMailFrom : "";
    g_config.to = szMailTo ? szMailTo : "";
    g_config.user = szMailUser ? szMailUser : "";
    g_config.credential = szMailPassword ? szMailPassword : "";
    g_config.port = iMailPort;
    g_config.options = nOptions;
    LeaveCriticalSection(&g_lock);

    if (nOptions & MAIL_OPTION_ENABLE)
    {
        StartThreadIfNeeded();
    }
    return 0;
}

int SendMail(HWND hResponse, bool bMatch, bool bMonitor_only, int iSeparateSMTP,
    char *sz1, char *sz2, char *sz3, char *sz4, char *sz5, char *sz6, char *sz7, char *szLabel)
{
    EnsureLock();
    if (hResponse)
    {
        EnterCriticalSection(&g_lock);
        g_response = hResponse;
        LeaveCriticalSection(&g_lock);
        SendMessageA(hResponse, LB_RESETCONTENT, 0, 0);
    }

    const int options = CurrentOptions();
    if (!(options & MAIL_OPTION_ENABLE))
    {
        return 0;
    }

    switch (options & MAIL_OPTION_MODES)
    {
        case MAIL_OPTION_MODE_FILTER:
            if (!bMatch || bMonitor_only) return 0;
            break;
        case MAIL_OPTION_MODE_MONITOR:
            if (!bMatch) return 0;
            break;
        case MAIL_OPTION_MODE_SELECTABLE:
            if (!bMatch || !iSeparateSMTP) return 0;
            break;
        default:
            break;
    }

    std::string payload;
    if ((options & MAIL_OPTION_ADDRESS) && sz1) payload += std::string(sz1) + " ";
    if ((options & MAIL_OPTION_TIME) && sz2) payload += std::string(sz2) + " ";
    if ((options & MAIL_OPTION_DATE) && sz3) payload += std::string(sz3) + " ";
    if ((options & MAIL_OPTION_MODE) && sz4) payload += std::string(sz4) + " ";
    if ((options & MAIL_OPTION_TYPE) && sz5) payload += std::string(sz5) + " ";
    if ((options & MAIL_OPTION_BITRATE) && sz6) payload += std::string(sz6) + " ";
    if ((options & MAIL_OPTION_MESSAGE) && sz7) payload += std::string(sz7) + " ";
    if ((options & MAIL_OPTION_LABEL) && szLabel) payload += std::string("- ") + szLabel + " ";

    if (payload.empty())
    {
        return 0;
    }

    return QueuePayload(payload) ? 0 : -1;
}
