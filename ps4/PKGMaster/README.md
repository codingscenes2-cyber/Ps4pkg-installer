# PKGMaster PS4 development build

This is an early PS4-native development build. It has a visible menu and a first USB package-library screen, but it is not yet a complete installer.

## Current native UI

- 1920x1080 framebuffer UI rendered through the OpenOrbis Scene2D / VideoOut path.
- DualShock input through ScePad and UserService.
- D-pad Up/Down moves through the main menu or package list.
- Cross opens Package Library and marks a selected package.
- Circle returns to the main menu.
- Triangle rescans USB storage from the package-library screen.

## Current package-library slice

The Package Library screen scans the root of the standard PS4 USB mount points: /mnt/usb0 through /mnt/usb3. It lists up to 64 regular files with a .pkg extension and displays their names and sizes.

This first scanner is non-recursive. It does not yet scan the internal HDD, parse package metadata, queue downloads, or install packages. Selecting a PKG only reports its name; native installer handoff is not connected.

## Build and package validation

The GitHub Actions workflow downloads the pinned OpenOrbis v0.5.4 toolchain, verifies its SHA-256, cross-compiles the native entry point, creates eboot.bin and builds a fake PKG.

Before uploading the artifact, CI:
- Parses the PKG entry table.
- Extracts eboot.bin from the package filesystem.
- Extracts and checks the PARAM.SFO and ICON0_PNG metadata entries.

## Build locally

Install the OpenOrbis PS4 Toolchain and its documented prerequisites, then set OO_PS4_TOOLCHAIN to the toolchain root (the directory containing link.x). Ensure clang++, ld.lld, and the Linux tools in bin/linux are executable.

    python3 tools/make_icon.py
    make clean all

The resulting package is named:

    IV0000-PKGM00001_00-PKGMASTER0000001.pkg

## Console testing

A successful CI build proves that the OpenOrbis cross-compile and package creation pipeline works. It does **not** prove that the app installs or runs on every firmware or GoldHEN version. Firmware 14.00 and the user's console setup still require hardware testing.

For this milestone, verify:
1. The package installs through GoldHEN's package installer.
2. The app launches to the PKGMaster menu.
3. D-pad Up/Down moves the menu highlight.
4. Cross opens Package Library.
5. With a USB drive containing PKG files attached, the library lists those files; Triangle rescans.
6. Circle returns to the main menu.
7. Selecting a PKG does not install it yet; it only displays a status message.

If video initialization or USB directory access fails, report that separately from a package installation failure.
