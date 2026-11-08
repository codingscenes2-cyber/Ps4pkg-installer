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
