# PKGMaster 0.2.0 — Native installer development preview

> **Status: development preview. CI build verified; physical PS4 installation testing still required. Not a stable release.**

## What's new

This milestone adds the first native local-PKG installation path to PKGMaster.

- AppInstUtil and BGFT module loading and service initialization.
- File-descriptor validation of selected package files: regular-file check, unchanged size and PS4 PKG CNT magic.
- AppInstUtil title-ID inspection, cross-checked against parsed PARAM.SFO metadata when available.
- BGFT local-storage task registration and explicit task start.
- BGFT progress polling with a dedicated confirmation and progress/error UI.
- Package Library scans USB mount roots /mnt/usb0–/mnt/usb3 and /data/pkg, using descriptor metadata.
- The progress UI distinguishes BGFT transfer completion from confirmed final installation.

## Build validation

The PS4 OpenOrbis workflow completed successfully on commit ddf9442a45bb7e10944fcdde3f1349b4c573b793.

- [PS4 OpenOrbis build and PKG verification — run #42504984252](https://github.com/codingscenes2-cyber/Ps4pkg-installer/actions/runs/42504984252)
- [Portable host validation — run #42504984179](https://github.com/codingscenes2-cyber/Ps4pkg-installer/actions/runs/42504984179)
- [Download the pkgmaster-ps4-boot-test artifact](https://github.com/codingscenes2-cyber/Ps4pkg-installer/actions/runs/42504984252/artifacts/13626644678)

CI confirms the PS4 cross-compile, package creation, package parsing/extraction checks and host tests. It does not exercise privileged BGFT/AppInstUtil behavior on a physical console.

## Test checklist

1. Install the development PKG through the package-install method supported by your own test environment.
2. Launch PKGMaster and open Package Library.
3. Place an authorized test PKG in the root of a USB drive or /data/pkg.
4. Select it and inspect the displayed metadata.
5. Press Cross to open the confirmation screen, then Cross again to submit the BGFT task.
6. Observe progress and separately confirm the final result in the PS4 system notifications/download UI.

Keep the USB drive connected while a task is using a package stored on it. Report module initialization errors, task registration errors, progress errors and final install failures separately, including the native error code.

## Known limitations

- Firmware 14.00 and the target GoldHEN release have not yet been hardware-qualified.
- BGFT task registration and progress reporting are compile-verified only until a console test succeeds.
- The UI does not yet provide task cancellation or recovery of BGFT tasks after restarting PKGMaster.
- Download Queue, Phone Transfer and Settings remain unconnected.
- Storage scanning is non-recursive and capped at 64 entries.
- Package metadata display may be incomplete for PKGs whose SFO entry cannot be read by the current parser; AppInstUtil performs the platform title-ID check before submission.

## Scope and safety

PKGMaster hands authorized local package files to the PS4's own package services. It does not bypass package signing, software licensing or entitlement checks. Use only packages you own or are authorized to install.
