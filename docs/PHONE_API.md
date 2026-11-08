# Phone pairing and local API contract

This contract is documentation only; the local HTTP service is not live yet.

## Pairing lifecycle
1. User explicitly enables Phone Link on the PS4.
2. Bind only to the LAN interface and generate a cryptographically secure, single-use pairing secret with a short expiry.
3. QR URL: `http://<console-lan-ip>:<port>/pair#<secret>`. The page consumes the fragment and POSTs it once.
4. Exchange it for a short-lived session token; invalidate the pairing secret.
5. Require the token for every mutation and support revocation on-console.
6. Stop the listener on app exit.

## Endpoints
| Method | Path | Purpose |
|---|---|---|
| POST | /api/v1/pair | Exchange one-time secret |
| GET | /api/v1/status | Console status/capabilities |
| GET | /api/v1/downloads | Queue snapshot |
| POST | /api/v1/downloads | Submit URL |
| POST | /api/v1/downloads/{id}/pause | Pause |
| POST | /api/v1/downloads/{id}/resume | Resume |
| DELETE | /api/v1/downloads/{id} | Cancel/remove |
| GET | /api/v1/library | App-managed package metadata |
| POST | /api/v1/installs/{id} | Request install handoff |

Mutating requests require `Authorization: Bearer <session-token>`. The browser must not report success until the service confirms it.

## Security
- LAN-only, explicit opt-in; never expose WAN.
- Resolve hosts and block loopback, link-local, multicast and private destinations by default.
- Re-check each redirect target after DNS resolution.
- Reject URL credentials and non-HTTP protocols.
- Never accept arbitrary filesystem paths from phone requests.
- Do not log tokens, passwords or signed URL query strings.
- Use platform cryptographic randomness, not timestamps or ordinary PRNG.


## Pairing credential rules

The portable core now contains `PhonePairingAuth`, an in-memory one-time pairing and bearer-session state machine. The LAN server integration must generate two distinct 256-bit tokens using an operating-system cryptographically secure random source: one short-lived pairing secret and one session token. The QR code may contain only the pairing secret plus the local pairing endpoint; never include the session token. Pairing secrets expire within five minutes and are consumed on the first successful exchange. Session lifetime is bounded to 30 days, and the server must call `authenticate` for every protected request and `revoke` when pairing is disabled or the service stops. The class does not generate entropy, create QR codes, open sockets, or implement an HTTP server; callers must provide CSPRNG tokens and enforce LAN-only binding, origin/CSRF checks, TLS or a clearly documented trusted-LAN threat model, request limits, and rate limiting.


The implementation now provides `generate_secure_token_hex` and `PhonePairingAuth::arm_random`. On Windows it uses the system-preferred BCrypt random provider; on POSIX/Orbis targets it reads exactly 32 bytes from `/dev/urandom`, handling interrupted reads. Failure is reported rather than falling back to a predictable generator. The returned pairing secret is 64 lowercase hexadecimal characters (256 bits); the session token is separately generated and remains internal to the auth object until exchange.


## HTTP request parser

The portable phone-service core now includes a strict single-request HTTP/1.1 parser. It accepts only GET/POST origin-form targets, requires Host, normalizes header names, rejects duplicate headers, folded lines, transfer-encoding, unsupported methods/versions, malformed lengths, oversized requests/bodies and trailing/pipelined bytes. Default limits are 128 KiB per request, 16 KiB of headers, 64 KiB of body and 32 headers. This parser consumes one complete in-memory request; a future socket server must enforce read timeouts, connection limits, peer/LAN binding, route authorization, response framing, and rate limiting.
