# PKGMaster

A modular PS4 homebrew package-library and download manager prototype.

> Status: engineering scaffold, not a production installer. Portable queue/state logic, versioned queue persistence, range planning/response validation, URL policy, capability gating, and a single-stream libcurl HTTP/HTTPS downloader are implemented and host-tested. Parallel range transport, persistent partial-file resume, authenticated phone service, QR pairing, PS4 UI/runtime adapters, archive extraction, and native installer bridge remain incomplete. No installable PS4 PKG has been built or hardware-tested.

## Target and compatibility
- Owner-reported target: PS4 firmware 14.00. This is not a verified compatibility claim.
- Compatibility strategy: capability-based, not tied to one GoldHEN release. Unknown versions may launch portable parts; platform features stay disabled until their adapters report the required capability.
- GoldHEN release, console model, and hardware results have not been recorded.
- Host HTTP transport uses libcurl. A PS4 libcurl port and its TLS/certificate integration must be separately verified before enabling it in a PS4 build.

See docs/COMPATIBILITY.md and docs/IMPLEMENTATION.md.

## Feature status
| Area | Current state |
|---|---|
| Download queue | Portable state model with pause/resume/retry/cancel |
| Queue persistence | Versioned snapshot load/save; metadata only |
| HTTP/HTTPS downloads | Single-stream libcurl transport, size cap, progress, cancellation, redirect refusal, temporary-file publish |
| Multi-range downloads | Overflow-safe planner, strict Content-Range parser and response validation; parallel transport pending |
| Resume metadata | Versioned atomic manifest stores a policy-checked source URL, strong ETag, total size and chunk completion; pending-range and remote-identity helpers added; partial-file resume orchestration pending |
| Phone submission | Static web UI; authenticated HTTP service pending |
| QR pairing | API contract only; generation and session exchange pending |
| Native PS4 UI/controller | Backend boundary only |
| Native package installation | Not implemented; verified bridge required |
| USB/HDD scanning | Adapter pending |
| RAR/multi-volume extraction | Not implemented |
| Firmware/GoldHEN compatibility | Capability-gating core added; runtime probing pending |
| Installable PKG | Not built or hardware-tested |

## Build
Install CMake, a C++17 compiler and libcurl development files. Configure with CMake; when libcurl is found the HTTP transport is enabled. The host CMake target is not a PS4 build. PS4 packaging must follow a verified OpenOrbis sample and a separately validated PS4-compatible network/TLS stack.

## Security
The downloader blocks private/local resolved socket destinations on POSIX, disables environment proxies, verifies TLS certificates, rejects redirects pending per-hop URL validation, applies a maximum response size, and publishes via a temporary file only after a complete successful response. Pairing requires a random one-time token, short expiry and LAN-only binding by default. Never expose unauthenticated filesystem access. Reject archive traversal and decompression bombs. Native installation requires an explicit validated adapter; no undocumented GoldHEN/Mira function is assumed.

No third-party SDK binaries are vendored. Choose distribution terms before adding a license.
