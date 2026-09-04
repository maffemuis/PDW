#ifndef PDW_SMTP_PROTOCOL_H
#define PDW_SMTP_PROTOCOL_H

#include <string>

namespace pdw
{
enum class SmtpTlsMode
{
    Plain,
    StartTls,
    ImplicitTls
};

SmtpTlsMode SelectSmtpTlsMode(bool tls_required, int port);
std::string Base64Encode(const std::string &input);
std::string BuildXOAuth2InitialResponse(const std::string &user, const std::string &access_token);
bool HasSmtpCapability(const std::string &capabilities, const char *needle);
std::string SanitizeSmtpHeaderValue(const std::string &value);
}

#endif
