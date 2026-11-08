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


## Live LAN server core

`PhoneLanServer` is now an opt-in HTTP/1.1 listener. The caller must explicitly supply a private RFC1918 IPv4 interface and nonzero port; wildcard, loopback, link-local and public bind addresses are rejected. Startup creates a five-minute one-time pairing code using OS-backed randomness, and the service accepts one bounded request per connection with receive/send timeouts. `POST /api/v1/pair` accepts the code as a `text/plain` body and returns the separately generated bearer session token; all other routes require that token and are delegated to the supplied application handler. Stopping the service closes the listener and revokes the session. This initial service is intentionally single-client-at-a-time and HTTP plaintext: use only on a trusted isolated LAN until a TLS transport is integrated. The app must display the pairing secret locally and must never put the session token in a QR code. The generic handler must implement route-specific authorization and must not expose arbitrary filesystem paths or installation commands without a separate safety gate.


## Connected API controller

`PhoneApiController` now supplies concrete authenticated routes for service status, the download queue, package-library metadata, and download submission/control. Phone clients submit only a URL as a `text/plain` body; the controller validates it with the shared download URL policy, accepts `.pkg` filenames, creates a destination beneath the configured downloads directory, and never accepts a client-supplied filesystem path. The background worker drives `DownloadCoordinator`; queue listing returns state and byte progress without exposing local paths or source URLs. Pause, resume, retry and cancellation are wired to the queue/coordinator. The library endpoint reads the bounded metadata index and returns title metadata only. Install requests intentionally return 501 until a verified native installer handoff exists. The worker may be disabled for deterministic tests; production must stop `PhoneLanServer` before destroying the controller because the server callback captures it.


## Self-hosted phone web UI

The LAN listener now serves a small same-origin mobile web UI at \`/\` and \`/pair\`, plus its local \`/app.js\` script. Opening the pairing URL places the one-time secret in the URL fragment, which is not sent in the HTTP request; the page consumes it once at \`/api/v1/pair\`, stores the returned bearer token in tab-scoped \`sessionStorage\`, removes the fragment from the visible URL, and then loads status, downloads and library data. The UI supports adding \`.pkg\` URLs and download pause/resume/retry/cancel. It uses no external scripts, fonts or image hosts, builds rows with \`textContent\`, and the server adds restrictive content-security, no-referrer and frame-denial headers. \`make_phone_pairing_url\` validates the LAN address and pairing token and creates the fragment-based URL for the console UI to display or encode as a QR code. This PR does not yet generate the QR matrix or draw it in the native PS4 UI.


## QR code generation

The pairing URL can now be rendered locally as a self-contained SVG QR code using a vendored copy of Project Nayuki's C++ QR Code generator (MIT License; attribution and notice retained in `third_party/QR-Code-generator`). The wrapper uses quartile error correction, enforces a 512-byte payload cap, restricts render scale/quiet-zone bounds, and emits only static SVG geometry with a white quiet-zone background. Pairing payloads remain local to the console; no external QR service is contacted. The native PS4 UI still needs to call `make_phone_pairing_url` followed by `generate_phone_pairing_qr_svg` and display the returned SVG.


## Storage discovery

The portable storage adapter probes only caller-configured roots, such as the internal app data directory and USB mount points supplied by the platform startup layer. It reports capacity, available bytes and writability, skips missing/unmounted roots, rejects malformed or duplicate candidate IDs, and does not assume hard-coded Orbis mount paths. The phone API exposes volume IDs/labels/capacity but not mount paths. Download paths are derived from a sanitized filename under the configured download root through `make_storage_child_path`; clients cannot select arbitrary paths. The actual Orbis startup layer must supply verified mount candidates and choose the download root.
