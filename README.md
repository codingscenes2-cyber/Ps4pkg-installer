# PKGMaster

**A PS4 homebrew package-library and download-manager project.**

PKGMaster is being built to make managing PlayStation 4 package files easier: discover PKG files on storage, inspect package information, manage downloads, verify files, and eventually hand compatible packages to the console's native installer flow. The project has a portable C++ core and an early OpenOrbis-built PS4 application.

> **Development status — early preview.** A PS4-targeted boot-test PKG has been built and structurally inspected in CI. The native application now contains a 1080p menu, controller navigation, and an initial USB PKG browser. It is not a finished installer: selecting a package does not install it, the download and phone-transfer screens are not connected, and real-console behavior has not yet been confirmed. Firmware 14.00 / GoldHEN compatibility is unverified.

## What exists today

### PS4 application preview

- Native PS4 application entry point cross-compiled with the OpenOrbis toolchain.
- 1920×1080 menu rendered through the OpenOrbis Scene2D / VideoOut path.
- DualShock input through ScePad and UserService.
- Main menu sections for Package Library, Download Queue, Phone Transfer, and Settings.
- Initial USB package browser scans `/mnt/usb0` through `/mnt/usb3` for regular `.pkg` files (root directory only; non-recursive), showing up to 64 entries and their sizes.
- D-pad navigation, Cross to open/select, Circle to return, and Triangle to rescan the USB package list.

The menu is a working development foundation, not proof that every screen or operation is complete. The other sections are placeholders until their platform adapters are connected.

### Portable C++ core

| Area | Current capability |
|---|---|
| Download queue | Explicit state transitions, pause/resume/retry/cancel model and versioned queue persistence |
| HTTP downloads | Single-stream libcurl transport with TLS verification, cancellation, progress, limits and staged publication |
| Parallel downloads | Bounded 2–8 worker range downloads, strong-ETag identity checks, exact range validation and resume checkpoints |
| Recovery and integrity | Resume manifest with per-chunk CRC32; optional whole-file SHA-256 when a trusted expected digest is supplied |
| Download coordinator | Connects queue state to probing, transport selection and a ready-to-install state; run from a worker thread |
| Archive extraction | Optional libarchive extraction with staging, path/link/special-file rejection and resource limits |
| PKG metadata | Bounded PARAM.SFO parser for fields such as TITLE, TITLE_ID, APP_VER, CATEGORY and SYSTEM_VER |
| Compatibility | Capability model intended to gate features on successfully initialized platform adapters |

The portable core and the PS4 UI are at different integration stages. A feature existing in the portable library does not automatically mean it is available in the PS4 application.

## What is not ready yet

- Native PS4 package-install handoff. PKGMaster does **not** currently install a selected PKG.
- Download Queue integration with the PS4 UI and PS4-native network/storage adapters.
- Phone-to-console transfer service, authenticated pairing and live QR workflow.
- Recursive USB scanning, internal HDD scanning, full package metadata display and persistent library indexing.
- Trusted package-signature/authenticity verification.
- Full target-device validation across PS4 models, firmware versions and GoldHEN releases.

## How a PS4 PKG works

A PS4 `.pkg` is a structured package container, not simply a renamed ZIP. It carries package metadata and the files that the console's package system needs to process. For a homebrew application package, the important pieces in this project's build include:

1. **Package metadata (`PARAM.SFO`)** — identifies the application to the system, including its title, title ID and application version.
2. **Application icon (`ICON0_PNG`)** — supplies the icon used by the system interface.
3. **Application payload (`uroot/eboot.bin`)** — contains the PS4 executable produced by the cross-compilation step.
4. **Package layout and metadata table** — describes the entries and their placement inside the PKG container.

At a high level, the build pipeline is:

```text
C/C++ source + OpenOrbis SDK
          |
          v
     PS4 ELF / eboot
          |
          +---- generated icon + PARAM.SFO + package layout
          |
          v
     OpenOrbis PkgTool.Core
          |
          v
       .pkg file
          |
          v
 CI parses entries and extracts selected files
          |
          v
 User transfers package to PS4 and tests installation/launch
```

The console's package installer is responsible for accepting and installing a compatible package. Building a PKG and successfully parsing it on a computer do not, by themselves, prove that the console will accept it, that the application will launch, or that the app can install other packages. Package signing/format requirements and firmware policy still apply.

## How the current PKG is built

The workflow [PS4 OpenOrbis build](.github/workflows/ps4-openorbis-build.yml):

1. Checks out the project on Ubuntu.
2. Downloads the pinned OpenOrbis PS4 toolchain and verifies its SHA-256 checksum.
3. Prepares an isolated OpenSSL 1.1 compatibility runtime for the legacy packaging utility.
4. Generates the application icon.
5. Cross-compiles the PS4 application and creates `eboot.bin`.
6. Packages the application into `IV0000-PKGM00001_00-PKGMASTER0000001.pkg`.
7. Uses PkgTool.Core to inspect the package entries, extract the application payload and extract the SFO/icon entries.
8. Runs a host-side metadata inspection and uploads the build artifacts.

**What CI proves:** the selected source revision cross-compiles and the packaging tool can create and inspect the resulting package in the build environment.

**What CI does not prove:** installation on a physical PS4, successful launch, controller behavior on hardware, USB access on every system, GoldHEN compatibility, or native package installation from inside PKGMaster.

## Get the latest development build

The current boot-test build is attached to [GitHub Actions run #42503211258](https://github.com/codingscenes2-cyber/Ps4pkg-installer/actions/runs/42503211258).

- [Download the `pkgmaster-ps4-boot-test` artifact](https://github.com/codingscenes2-cyber/Ps4pkg-installer/actions/runs/42503211258/artifacts/13625988654)
- [View the build and validation logs](https://github.com/codingscenes2-cyber/Ps4pkg-installer/actions/runs/42503211258)

The artifact is a development/test build, not a stable public release. GitHub Actions artifacts expire; rebuild from the workflow if the artifact is no longer available.

### Console test checklist

1. Install the PKG using the package-install method appropriate for your own jailbroken test setup.
2. Check whether PKGMaster appears in the system menu and launches.
3. Confirm the main menu renders.
4. Test D-pad navigation, Cross, and Circle.
5. Attach a USB drive containing a few `.pkg` files in its root directory; open Package Library and press Triangle to rescan.
6. Confirm that package names and sizes appear. Remember: selecting an entry is not an install operation yet.

Report installation failure, launch failure, graphics failure, input failure and USB-scan failure separately; they point to different parts of the stack.

## Build the portable core on a computer

Requirements: CMake 3.20+, a C++17 compiler and libcurl development files. Libarchive is optional.

```sh
cmake -S . -B build -DPKGMASTER_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Build the PS4 application

Install the OpenOrbis PS4 Toolchain and its documented prerequisites. Set `OO_PS4_TOOLCHAIN` to the toolchain root (the directory containing `link.x`), then run:

```sh
python3 ps4/PKGMaster/tools/make_icon.py
make -C ps4/PKGMaster clean all
```

The generated PKG is written under `ps4/PKGMaster/`. See [the PS4 build notes](ps4/PKGMaster/README.md) and [the package/build walkthrough](docs/PKG_FORMAT_AND_BUILD.md).

## Project guide

- [PKG format and build pipeline](docs/PKG_FORMAT_AND_BUILD.md)
- [Development preview release notes](docs/RELEASE_NOTES_0.1.0-boot-test.md)
- [Implementation status](docs/IMPLEMENTATION.md)
- [Compatibility approach](docs/COMPATIBILITY.md)
- [Phone API design](docs/PHONE_API.md)

## Security and integrity model

- HTTP redirects are rejected in current transports; a future redirect implementation must validate every hop and resolved destination.
- TLS peer and hostname verification are enabled in libcurl builds.
- Downloads are bounded and staged before publication.
- Resume requires a matching strong ETag and object length. Chunk CRC32 detects accidental corruption; it is not authenticity protection.
- SHA-256 verification is only meaningful when the expected digest comes from a trusted source.
- Archive extraction rejects unsafe paths and links and applies resource limits.
- Phone transfer must not expose filesystem actions until short-lived pairing, authenticated sessions, LAN binding and request limits are implemented.

## Dependencies and licensing

CMake discovers libcurl and libarchive optionally. The OpenOrbis toolchain is downloaded by CI rather than vendored. Review third-party licenses and add a project license before redistributing binaries.
