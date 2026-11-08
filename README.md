# PKGMaster

A modular PS4 homebrew package-library and download manager prototype.

> **Status: engineering scaffold — not a production installer.** This first revision establishes the project layout, host-side logic contracts, mobile UI, build metadata and safety boundaries. It does not yet contain a verified Orbis UI backend, GoldHEN-specific installer bridge, working PS4 HTTP daemon, RAR password recovery, or a tested `.pkg`. No binary is being represented as built.

## Target and compatibility

- Intended target: PS4 / Orbis homebrew, with firmware 14.00 as a **target to validate**, not a confirmed compatibility claim.
- Earlier project discussion mentioned a console on firmware 12.xx. Test on the exact firmware and GoldHEN release before relying on this application.
- Build foundation: [OpenOrbis PS4 Toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain). Its own documentation describes the compiler, linker, PS4 libraries, ELF-to-SELF and PKG packaging tools. A successful build does not prove runtime compatibility on every firmware.

## Feature status

| Area | Current state |
|---|---|
| Download queue model | Initial portable implementation |
| Multi-range downloader | Interface and validation plan; transport implementation pending |
| Phone link submission | Responsive static web UI; authenticated HTTP service pending |
| QR pairing | API contract pending |
| Native PS4 UI/controller | Backend boundary only |
| Native background package install | **Not implemented**; requires a verified supported system bridge |
| USB/HDD library scanning | Storage abstraction pending |
| RAR/multi-volume extraction | Not implemented |
| Password resolver | Manual-password design only; no brute-force dictionary feature |
| Firmware 14.00 | Not tested |
| Installable PKG | Not built |

## Architecture

- `src/core`: queue, task state and validation, independent of Orbis UI.
- `src/platform`: platform-specific filesystem, network and installer adapters.
- `src/ui`: PS4 rendering/input adapter.
- `src/server` + `web/`: local phone management service and mobile UI.
- `src/archive`: archive inspection/extraction adapter.
- `tests/`: host-side tests for portable logic.

The platform adapters must fail closed when a capability is unavailable. In particular, a downloaded file must never be marked installed merely because it was queued.

## Build prerequisites

1. Install a release of the [OpenOrbis toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain) and the LLVM clang/lld versions it supports.
2. Set `OO_PS4_TOOLCHAIN` to the extracted toolchain root and add its `bin` directory to `PATH`.
3. Follow the toolchain's current sample project and packaging workflow. Do not assume generic desktop CMake settings are sufficient for Orbis.
4. Add and validate platform libraries only after confirming they exist in the selected toolchain build.
5. Generate a valid `eboot.bin`, `sce_sys/param.sfo`, 512x512 opaque 24-bit `sce_sys/icon0.png`, and GP4/PKG using the toolchain's packaging utilities.

See [OpenOrbis README](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain) and its [homebrew build guide](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/docs/MD/Building%20Homebrew.md).

## Security notes

- Pairing must use a random one-time token, short expiry, and explicit LAN-only binding by default.
- Never expose an unauthenticated file browser or arbitrary filesystem path API.
- Accept only HTTP(S) URLs; block loopback, link-local and private-network destinations unless the user explicitly allows a local source. Re-check redirects to prevent SSRF.
- Verify downloaded byte count and optional SHA-256 before offering installation.
- Treat archive filenames and paths as untrusted; reject traversal, absolute paths, symlinks and decompression bombs.
- Password recovery is limited to a password supplied by the user. Automated password-dictionary cracking is intentionally not included.
- Native installation is delegated to an explicit, separately validated adapter. There is no invented GoldHEN/Mira function call in this codebase.

## Roadmap

1. Compile the portable core on desktop and add unit tests.
2. Port the app entry point and controller-driven UI using a verified OpenOrbis sample.
3. Implement the PS4 storage/network adapters with supported APIs.
4. Implement authenticated QR pairing and phone-to-console job submission.
5. Implement resumable range downloads with server capability checks and integrity validation.
6. Integrate a verified package-install handoff, then test queue/error recovery on real hardware.
7. Add archive support and complete library metadata scanning.
8. Build, sign/package as appropriate for the homebrew environment, and publish a test PKG only after real console validation.

## License

No third-party SDK binaries are vendored. Add a project license after choosing the distribution terms for the complete application.
