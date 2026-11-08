# PKGMaster

A modular PS4 homebrew package-library and download manager prototype.

> Status: engineering scaffold, not a production installer. Portable queue/state logic, versioned queue persistence, range planning/response validation, URL policy, and capability gating are implemented and covered by host tests. The real HTTP transport, authenticated phone service, QR pairing, PS4 UI/runtime adapters, archive extraction, and native installer bridge remain incomplete. No installable PS4 PKG has been built or hardware-tested.

## Target and compatibility
- Owner-reported target: PS4 firmware 14.00. This is not a verified compatibility claim.
- Compatibility strategy: capability-based, not tied to one GoldHEN release. Unknown versions may launch portable parts; platform features stay disabled until their adapters report the required capability.
- GoldHEN release, console model, and hardware results have not been recorded.
- Build foundation: OpenOrbis PS4 Toolchain. A successful host build does not prove runtime compatibility on every firmware.

See docs/COMPATIBILITY.md.

## Feature status
| Area | Current state |
|---|---|
| Download queue | Portable state model with pause/resume/retry/cancel |
| Queue persistence | Versioned snapshot load/save, validation and same-filesystem temp-file rename |
| Multi-range downloads | Range planner and response validation; HTTP transport pending |
| Phone submission | Static web UI; authenticated HTTP service pending |
| QR pairing | API contract only; generation and session exchange pending |
| Native PS4 UI/controller | Backend boundary only |
| Native package installation | Not implemented; verified bridge required |
| USB/HDD scanning | Adapter pending |
| RAR/multi-volume extraction | Not implemented |
| Firmware/GoldHEN compatibility | Capability-gating core added; runtime probing pending |
| Installable PKG | Not built or hardware-tested |

## Architecture
- src/core: portable queue, persistence, state and validation logic.
- src/platform: filesystem, network, installer and capability adapters.
- src/ui: PS4 rendering/input adapter.
- src/server and web: phone management service and UI.
- src/archive: archive inspection/extraction adapter.
- tests: host-side tests.

Adapters must fail closed when capabilities are unavailable. A downloaded file must never be marked installed merely because it was queued.

## Build
Install a compatible OpenOrbis toolchain and its supported LLVM versions. Follow the current upstream sample project, verify every API against the selected toolchain, and use its tools to create eboot.bin, param.sfo, icon, GP4 and PKG. The host CMake target is not a PS4 build.

## Security
Pairing requires a random one-time token, short expiry and LAN-only binding by default. Never expose unauthenticated filesystem access. Re-check redirects, verify download size and optional SHA-256, reject archive traversal and decompression bombs. Native installation requires an explicit validated adapter; no undocumented GoldHEN/Mira function is assumed.

No third-party SDK binaries are vendored. Choose distribution terms before adding a license.
