# PKGMaster

A modular PS4 homebrew package-library and download-manager project. The portable C++ core is actively developed and host-tested; this repository is **not yet an installable PS4 application**.

> **Current validation boundary:** host-side CMake build and tests only. No PS4 ELF/PKG has been produced, no PS4 hardware test has been performed, and native package installation is not implemented. The reported target is PS4 firmware 14.00; that is not a compatibility certification. GoldHEN release and console model are not yet recorded.

## Implemented portable components

| Component | Current implementation |
|---|---|
| Queue/state model | Explicit legal state transitions; queue, pause, resume, retry and cancel operations |
| Queue persistence | Versioned snapshots, transactional loading and validation; stores job metadata, not partial transfer state |
| URL policy | HTTP/HTTPS policy checks and rejection of private/local literal destinations; POSIX socket callbacks also reject private/local resolved destinations |
| Single-stream HTTP | libcurl transport, TLS verification, byte cap, optional receive-rate cap, cancellation, progress, speed/ETA callback, redirect refusal and temporary-file publication |
| Parallel HTTP | Bounded 2–8 worker range transport, strong ETag binding, exact Content-Range/body validation, per-range staging writes, resume checkpoints and aggregate speed/ETA callback |
| Interrupted-transfer recovery | Resume manifest v2 stores URL, strong ETag, object length, range map and CRC32 per completed chunk; re-probes identity and verifies recovered chunks before reuse |
| Whole-file integrity | Optional caller-supplied trusted SHA-256 digest before publishing either single-stream or parallel downloads |
| Download coordinator | Connects queue state to probe, parallel/single-stream selection, cancellation and ready-to-install state; invoke from a worker thread |
| Archive extraction | Optional libarchive-backed extraction with staging directory, path/link/special-file rejection and file/size/expansion limits |\n| PS4 package metadata | Bounded PARAM.SFO parser for TITLE, TITLE_ID, APP_VER, CATEGORY and SYSTEM_VER; library scanner/UI integration pending |
| Compatibility model | Runtime/capability data model and feature gating; this is not yet a complete PS4 runtime probe |

## Not yet implemented / not yet validated

- Authenticated LAN phone HTTP service, live QR pairing and browser-to-console submission.
- PS4-native UI, controller input adapter, USB/HDD discovery and package library scanner/index.
- Native package-installer handoff. `ReadyToInstall` means the file is downloaded and verified according to configured checks; it does **not** mean the package was installed.
- PS4-targeted libcurl/TLS integration, OpenOrbis application build, installable PKG and on-console tests.
- Multi-volume RAR behavior on the target platform; support depends on the linked libarchive build and remains unverified.
- Trusted package-signature verification. CRC32 is only for detecting accidental local chunk corruption; SHA-256 is meaningful only when the expected digest comes from a trusted source.

## Build and test on a host

Requirements: CMake 3.20+, C++17 compiler, libcurl development package. Libarchive is optional; without it, archive extraction reports `Unsupported`.

```sh
cmake -S . -B build -DPKGMASTER_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The GitHub Actions workflow runs these portable host checks. A successful host build does not demonstrate PS4 ABI compatibility, GoldHEN compatibility, or native installer support.

## Compatibility strategy

PKGMaster is designed around capability detection instead of assuming that a firmware or GoldHEN version string guarantees an API. Runtime version fields are diagnostic. Features should remain unavailable until a corresponding adapter has successfully initialized and validated its capability. Unknown versions must not be treated as proof of compatibility.

See [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md), [docs/IMPLEMENTATION.md](docs/IMPLEMENTATION.md), and [docs/PHONE_API.md](docs/PHONE_API.md).

## Security notes

- Redirects are rejected in the current HTTP transports. A future redirect implementation must validate every hop and resolved address.
- TLS peer and hostname verification are enabled in libcurl builds.
- Private/local resolved destinations are blocked by POSIX socket callbacks; non-POSIX builds require equivalent platform enforcement before accepting untrusted URLs.
- Downloads are bounded and staged before publication.
- Resume requires a matching strong ETag and object length. Manifest CRC32 detects accidental local corruption; it is not cryptographic authenticity.
- SHA-256 verification is enabled only when the caller supplies a trusted expected digest.
- Archive extraction rejects unsafe paths and links and enforces configured resource limits.
- The phone-service design must use short-lived pairing, authenticated sessions, LAN binding and strict request limits before any filesystem action is exposed.

## Third-party dependencies and licensing

CMake discovers libcurl and libarchive optionally. No third-party SDK binaries are vendored. Confirm dependency licensing and choose a project license before distributing binaries.