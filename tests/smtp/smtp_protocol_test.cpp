#include "smtp_protocol.h"

#include <iostream>
#include <string>

namespace
{
int Fail(const char *message)
{
    std::cerr << message << std::endl;
    return 1;
}
}

int main()
{
    if (pdw::SelectSmtpTlsMode(false, 25) != pdw::SmtpTlsMode::Plain)
        return Fail("plain SMTP selection changed");
    if (pdw::SelectSmtpTlsMode(true, 587) != pdw::SmtpTlsMode::StartTls)
        return Fail("port 587 must use STARTTLS when TLS is required");
    if (pdw::SelectSmtpTlsMode(true, 465) != pdw::SmtpTlsMode::ImplicitTls)
        return Fail("port 465 must use implicit TLS");

    if (pdw::Base64Encode("user") != "dXNlcg==")
        return Fail("base64 contract mismatch");

    const std::string xoauth = pdw::BuildXOAuth2InitialResponse("u@example.com", "token");
    if (xoauth != "dXNlcj11QGV4YW1wbGUuY29tAWF1dGg9QmVhcmVyIHRva2VuAQE=")
        return Fail("XOAUTH2 initial client response mismatch");

    const std::string capabilities = "smtp.example\nSIZE 1000\nSTARTTLS\nAUTH LOGIN PLAIN XOAUTH2\n";
    if (!pdw::HasSmtpCapability(capabilities, "starttls"))
        return Fail("STARTTLS capability not detected");
    if (!pdw::HasSmtpCapability(capabilities, "XOAUTH2"))
        return Fail("XOAUTH2 capability not detected");
    if (pdw::HasSmtpCapability(capabilities, "CHUNKING"))
        return Fail("missing capability falsely detected");

    const std::string near_matches = "smtp.example\nXSTARTTLS\nAUTH XLOGIN NOXOAUTH2\n";
    if (pdw::HasSmtpCapability(near_matches, "STARTTLS"))
        return Fail("STARTTLS substring falsely detected as capability");
    if (pdw::HasSmtpCapability(near_matches, "LOGIN"))
        return Fail("LOGIN substring falsely detected as auth mechanism");
    if (pdw::HasSmtpCapability(near_matches, "XOAUTH2"))
        return Fail("XOAUTH2 substring falsely detected as auth mechanism");

    const std::string auth_equals = "smtp.example\nAUTH=LOGIN XOAUTH2\n";
    if (!pdw::HasSmtpCapability(auth_equals, "LOGIN")
        || !pdw::HasSmtpCapability(auth_equals, "XOAUTH2"))
        return Fail("AUTH= capability tokenization changed");

    const std::string unsafe_header = "PDW alert\r\nBcc: injected@example.com\x01";
    const std::string safe_header = pdw::SanitizeSmtpHeaderValue(unsafe_header);
    if (safe_header.find('\r') != std::string::npos
        || safe_header.find('\n') != std::string::npos
        || safe_header.find('\x01') != std::string::npos)
        return Fail("SMTP header sanitizer left control characters");
    if (safe_header != "PDW alert  Bcc: injected@example.com ")
        return Fail("SMTP header sanitizer contract mismatch");

    return 0;
}
