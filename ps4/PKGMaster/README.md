# PKGMaster PS4 development build

This is the first visible PS4-native UI milestone. It is a development shell, not the finished PKG manager.

## Current native UI

- 1920x1080 framebuffer UI rendered through the OpenOrbis sample Scene2D / VideoOut path.
- DualShock input through ScePad and UserService.
- D-pad Up/Down moves the selection.
- Cross selects a section and displays its current implementation status.
- Circle resets the status line.

The four menu sections are placeholders. Package scanning, USB/HDD browsing, download management, phone transfer, settings persistence and native installer handoff are not connected yet.

## Build and package validation

The GitHub Actions workflow downloads the pinned OpenOrbis v0.5.4 toolchain, verifies its SHA-256, cross-compiles the native entry point, creates eboot.bin and builds a fake PKG.

Before uploading the artifact, CI:
- Parses the PKG entry table.
- Extracts eboot.bin from the package filesystem.
- Extracts and checks the PARAM.SFO and ICON0_PNG metadata entries.

The latest successful workflow artifact is linked from PR #33.

## Build locally

Install the OpenOrbis PS4 Toolchain and its documented prerequisites, then set OO_PS4_TOOLCHAIN to the toolchain root (the directory containing link.x). Ensure clang++, ld.lld, and the Linux tools in bin/linux are executable.

    python3 tools/make_icon.py
    make clean all

The resulting package is named:

    IV0000-PKGM00001_00-PKGMASTER0000001.pkg

## Console testing

A successful CI build proves that the OpenOrbis cross-compile and package creation pipeline works. It does **not** prove that the app installs or runs on every firmware or GoldHEN version. Firmware 14.00 and the user's console setup still require hardware testing.

For this milestone, verify:
1. The package appears in the GoldHEN package installer and installs.
2. The app launches to the PKGMaster menu.
3. D-pad Up/Down moves the highlight.
4. Cross changes the status message; Circle resets it.
5. The PS button can return to the system menu and close the app.

If video initialization fails, the app currently stays alive without a visible menu; report that separately from an installation failure.
