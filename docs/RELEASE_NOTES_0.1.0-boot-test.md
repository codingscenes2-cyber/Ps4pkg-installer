# PKGMaster 0.1.0 — PS4 boot-test development preview

> **Status: development preview / hardware validation pending. Not a stable release.**

## Summary

This milestone establishes a real PS4-targeted build pipeline for PKGMaster and packages an early native application using the OpenOrbis PS4 toolchain.

The current application foundation includes:
- A 1920×1080 native menu rendered through the OpenOrbis Scene2D / VideoOut path.
- DualShock input through ScePad and UserService.
- Main menu entries for Package Library, Download Queue, Phone Transfer and Settings.
- An initial USB package browser that scans `/mnt/usb0` through `/mnt/usb3` (root only), lists up to 64 regular `.pkg` files and displays names and sizes.
- D-pad navigation, Cross selection/open, Circle back and Triangle USB rescan.

## Build validation

GitHub Actions run [#42503211258](https://github.com/codingscenes2-cyber/Ps4pkg-installer/actions/runs/42503211258) completed successfully. The workflow:
- Downloaded and checksum-verified OpenOrbis Toolchain v0.5.4.
- Cross-compiled the PS4 application and generated `eboot.bin`.
- Created the PKG using OpenOrbis PkgTool.Core.
- Enumerated package entries and extracted the executable payload.
- Extracted and checked the SFO metadata and icon entries.
- Uploaded the build artifact.

Artifact: [pkgmaster-ps4-boot-test](https://github.com/codingscenes2-cyber/Ps4pkg-installer/actions/runs/42503211258/artifacts/13625988654)

## Known limitations

- This is a boot-test / UI-foundation package, not a finished PKG manager.
- Selecting a listed package does not install it; native installer handoff is not connected.
- Download Queue, Phone Transfer and Settings are not fully integrated.
- USB scan is non-recursive and does not scan the internal HDD.
- Hardware behavior has not yet been confirmed. Firmware 14.00 and GoldHEN compatibility are unverified.
- Package creation and CI extraction checks do not prove console acceptance or successful launch.

## Test checklist

1. Install the PKG using the normal package-install method for the test setup.
2. Confirm the application appears and launches.
3. Check rendering and controller navigation.
4. Open Package Library with a USB drive containing test `.pkg` files in its root.
5. Use Triangle to rescan and Circle to return.

Please report install, launch, graphics, input and USB results separately.

## Next development priorities

1. Complete and harden the package library (metadata parsing, storage discovery and indexing).
2. Connect the download queue to PS4-native storage/network adapters.
3. Implement secure phone-to-console transfer and pairing.
4. Research and implement a verified native installer adapter; do not expose an install action until the platform handoff is validated.
5. Run hardware tests across documented firmware / GoldHEN combinations.
