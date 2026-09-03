# Modern SMTP transport

PDW now uses a modern SMTP transport while keeping the existing SMTP settings dialog and public `MailInit` / `SendMail` API.

## TLS behavior

When the existing **SSL** option is enabled, TLS is mandatory:

- port **465** uses implicit TLS;
- every other port (normally **587**) uses `EHLO -> STARTTLS -> TLS -> EHLO`;
- TLS 1.2 is the minimum accepted version;
- the SMTP hostname is sent through SNI and verified against the certificate;
- the peer certificate chain is verified using the Windows trusted root certificate store;
- if STARTTLS is required but not advertised, sending fails closed;
- authentication is never sent over a plaintext connection.

For modern client submission, port 587 with the existing SSL option enabled is the recommended configuration.

## OAuth2 / XOAUTH2

When **Enable Authentication** is selected, PDW can authenticate with SASL XOAUTH2. OAuth is selected when an access token is available from one of these sources, in this order:

1. `PDW_SMTP_OAUTH2_TOKEN_FILE` — path to a local file containing the current access token;
2. `PDW_SMTP_OAUTH2_TOKEN` — access token in the process environment;
3. the existing Password field with the transitional prefix `oauth2:<access-token>`.

The token-file form is recommended because an external token refresher can replace the file and PDW will read the current token for every new SMTP session without storing the bearer token in `PDW.ini`.

Set `PDW_SMTP_AUTH_MODE=oauth2` to require XOAUTH2 and fail closed if no token is available. Set `PDW_SMTP_AUTH_MODE=login` to explicitly keep legacy AUTH LOGIN for a server that still requires it.

Without an OAuth token or explicit auth mode, the existing username/password configuration continues to use AUTH LOGIN, but only inside verified TLS.

## Microsoft 365 example

Typical transport settings:

- Server: `smtp.office365.com`
- Port: `587`
- SSL: enabled (PDW uses STARTTLS on this port)
- Authentication: enabled
- Username: mailbox SMTP address
- OAuth access token: supplied using `PDW_SMTP_OAUTH2_TOKEN_FILE` or `PDW_SMTP_OAUTH2_TOKEN`

The SMTP server must advertise `AUTH XOAUTH2`. Token acquisition/refresh remains provider-specific and is deliberately outside the SMTP transport; PDW consumes a bearer token but does not store OAuth client secrets in its INI file.

## Security changes from the legacy transport

The old `utils/smtp.cpp` remains in the repository as historical reference but is no longer compiled. The modern transport removes credential-bearing debug output, reads the server greeting before sending commands, handles multiline EHLO responses, performs STARTTLS in protocol order, validates certificates/hostnames, supports IPv4/IPv6 via `getaddrinfo`, dot-stuffs DATA correctly, and emits MIME headers.
