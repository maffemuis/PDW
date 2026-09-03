#include "smtp_protocol.h"

#include <algorithm>
#include <cctype>

namespace pdw
{
SmtpTlsMode SelectSmtpTlsMode(bool tls_required, int port)
{
    if (!tls_required)
    {
        return SmtpTlsMode::Plain;
    }

    return port == 465 ? SmtpTlsMode::ImplicitTls : SmtpTlsMode::StartTls;
}

std::string Base64Encode(const std::string &input)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    for (size_t i = 0; i < input.size(); i += 3)
    {
        const unsigned int a = static_cast<unsigned char>(input[i]);
        const bool have_b = i + 1 < input.size();
        const bool have_c = i + 2 < input.size();
        const unsigned int b = have_b ? static_cast<unsigned char>(input[i + 1]) : 0;
        const unsigned int c = have_c ? static_cast<unsigned char>(input[i + 2]) : 0;

        output.push_back(alphabet[(a >> 2) & 0x3f]);
        output.push_back(alphabet[((a & 0x03) << 4) | ((b >> 4) & 0x0f)]);
        output.push_back(have_b ? alphabet[((b & 0x0f) << 2) | ((c >> 6) & 0x03)] : '=');
        output.push_back(have_c ? alphabet[c & 0x3f] : '=');
    }

    return output;
}

std::string BuildXOAuth2InitialResponse(const std::string &user, const std::string &access_token)
{
    std::string sasl;
    sasl.reserve(user.size() + access_token.size() + 24);
    sasl += "user=";
    sasl += user;
    sasl.push_back('\x01');
    sasl += "auth=Bearer ";
    sasl += access_token;
    sasl.push_back('\x01');
    sasl.push_back('\x01');
    return Base64Encode(sasl);
}

bool HasSmtpCapability(const std::string &capabilities, const char *needle)
{
    if (!needle || !needle[0])
    {
        return false;
    }

    std::string haystack = capabilities;
    std::string target = needle;

    std::transform(haystack.begin(), haystack.end(), haystack.begin(),
        [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    std::transform(target.begin(), target.end(), target.begin(),
        [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });

    return haystack.find(target) != std::string::npos;
}
}
