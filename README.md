# PKGMaster

**A native PS4 PKG library and installer project, with a portable C++ download core.**

PKGMaster is being developed to discover package files on USB/internal storage, inspect package metadata, submit selected local PKGs to the PS4 package service, track installation tasks, and later provide download and phone-transfer workflows.

> **Current status — early development build.** The OpenOrbis PS4 application now includes a native menu, controller navigation, USB/internal package scanning, PARAM.SFO inspection, and a first AppInstUtil/BGFT installation adapter. The latest PS4-targeted PKG compiles and passes CI package-structure checks. Actual BGFT task registration and final installation have **not** yet been verified on a physical console. Firmware 14.00 / GoldHEN compatibility remains unconfirmed.

## Current PS4 application

- 1920×1080 UI rendered through OpenOrbis Scene2D / VideoOut.
- DualShock input through ScePad and UserService.
- Package Library scans `/mnt/usb0` through `/mnt/usb3` and `/data/pkg` for regular `.pkg` files (root directories only; non-recursive; maximum 64 entries).
- File metadata is read through opened descriptors to avoid relying on path-based `stat/lstat` behavior that may be incomplete in some PS4 runtimes.
- Package details screen reads unencrypted `PARAM.SFO` fields when available.
- Native installer adapter initializes AppInstUtil and BGFT, validates the selected file's size and PS4 PKG magic, asks AppInstUtil for the title ID, registers a local-storage BGFT task, starts it, and polls progress.
- Install confirmation and progress/error screens are connected to the package details flow.

**Install controls:** Cross from the package details screen opens the confirmation screen. Cross again submits the task. Circle cancels before submission; after the task reaches a terminal state, Circle returns to package details.

### Important limitations

- BGFT/AppInstUtil behavior is compiled but not hardware-qualified yet.
- BGFT transfer completion is not treated as proof of final installation success; check the PS4 system notification/download result.
- Download Queue, Phone Transfer and Settings remain unconnected placeholders.
- USB/internal scans are non-recursive and limited to the first 64 package files.
- The SFO parser may not be able to display metadata from every package; AppInstUtil is asked to inspect the selected package before BGFT submission.
- No package-signature or license bypass is implemented. Use only PKGs you own or are authorized to install.

See [PS4 build and test notes](ps4/PKGMaster/README.md).

## Portable C++ core

| Component | Current implementation |
|---|---|
| Queue/state model | Explicit legal state transitions; queue, pause, resume, retry and cancel operations |
| Queue persistence | Versioned snapshots, transactional loading and validation |
| HTTP downloads | libcurl transport with TLS verification, cancellation, progress, limits, redirect refusal and staged publication |
| Parallel downloads | Bounded 2–8 worker range downloads with strong-ETag identity checks and resume checkpoints |
| Integrity | Per-chunk CRC32 for accidental corruption; optional whole-file SHA-256 when a trusted expected digest is supplied |
| Download coordinator | Queue-to-download orchestration and ready-to-install state; invoke from a worker thread |
| Archive extraction | Optional libarchive extraction with path/link/special-file rejection and resource limits |
| Package metadata | Bounded PARAM.SFO parser for TITLE, TITLE_ID, APP_VER, CATEGORY and SYSTEM_VER |
| Compatibility | Capability model intended to gate platform features on successfully initialized adapters |

Portable-core functionality is not automatically integrated into the PS4 UI. In particular, the download coordinator is not yet wired to the native BGFT installer flow.

## How local installation works

```text
USB /data/pkg package selection
             |
             v
    File descriptor checks
    - regular file
    - unchanged size
    - PS4 PKG magic
             |
             v
    AppInstUtil reads Title ID
             |
             v
       User confirms
             |
             v
    BGFT registers local-storage task
             |
             v
       BGFT starts task
             |
             v
    PKGMaster polls task progress
             |
             v
    Verify final result in PS4 system UI
```

PKGMaster does not implement its own package unpacker or replace the PS4 package manager. It hands a validated local package path to the system BGFT/AppInstUtil services. The console environment decides whether the package can be accepted and installed. A successful build is not proof of successful installation.

## Build and validation

The [PS4 OpenOrbis build workflow](.github/workflows/ps4-openorbis-build.yml) downloads and checksum-verifies OpenOrbis v0.5.4, cross-compiles the application, creates the PKG, parses the package entries, extracts the executable and checks the SFO/icon metadata.

The latest installer implementation passed:
- [PS4 OpenOrbis build and package verification](https://github.com/codingscenes2-cyber/Ps4pkg-installer/actions/runs/42504984252)
- [Portable host validation](https://github.com/codingscenes2-cyber/Ps4pkg-installer/actions/runs/42504984179)

These checks establish build and package structure only. They do not exercise the privileged BGFT service on a real PS4.

### Build the portable core

Requirements: CMake 3.20+, C++17 compiler, libcurl development package. Libarchive is optional.

```sh
cmake -S . -B build -DPKGMASTER_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### Build the PS4 application

Install OpenOrbis PS4 Toolchain v0.5.4 and its prerequisites. Set `OO_PS4_TOOLCHAIN` to the toolchain root (the directory containing `link.x`), then run:

```sh
python3 ps4/PKGMaster/tools/make_icon.py
make -C ps4/PKGMaster clean all
```

The generated package is written under `ps4/PKGMaster/`. See [PS4 build and test notes](ps4/PKGMaster/README.md).

## Security and compatibility

- Selected files are checked for regular-file status, unchanged size and PS4 PKG magic before submission.
- AppInstUtil's title ID is compared with parsed SFO metadata when that metadata is available.
- TLS verification and destination restrictions apply to the portable HTTP transports.
- Resume CRC32 detects accidental corruption, not malicious modification. SHA-256 only provides meaningful authenticity when the expected digest is trusted.
- No firmware or GoldHEN version is treated as a compatibility guarantee. Runtime capabilities must be initialized and hardware-tested.

See [compatibility notes](docs/COMPATIBILITY.md), [implementation status](docs/IMPLEMENTATION.md), and [phone API design](docs/PHONE_API.md).

## Dependencies and licensing

CMake discovers libcurl and libarchive optionally. The OpenOrbis toolchain is downloaded by CI rather than vendored. Review third-party licenses and add a project license before redistributing binaries.
