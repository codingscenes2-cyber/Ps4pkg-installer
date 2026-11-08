# PKGMaster PS4 boot smoke test

This is the first PS4-targeted build slice. It is intentionally a boot test, not the finished PKG manager.

## What it proves

- OpenOrbis compiles and links a native PS4 entry point.
- The build produces an Orbis eboot.bin.
- OpenOrbis package tools generate a .pkg containing the eboot, PARAM.SFO and an original generated icon.

The app prints startup diagnostics and stays alive. It has no visible native UI yet, and it does not browse storage, download packages, or invoke the system installer. A successful build is not proof of firmware 14.00 or GoldHEN compatibility; that requires an actual console test.

## Build locally

Install the OpenOrbis PS4 Toolchain and its documented prerequisites, then set OO_PS4_TOOLCHAIN to the toolchain root (the directory containing link.x). Ensure clang++, ld.lld, and the Linux tools in bin/linux are executable.

    python3 tools/make_icon.py
    make clean all

The resulting package is named:

    IV0000-PKGM00001_00-PKGMASTER0000001.pkg

## CI

The PS4 OpenOrbis build GitHub Actions workflow downloads the pinned OpenOrbis v0.5.4 release, builds the package, and uploads the .pkg, eboot.bin, and GP4 project as a short-lived Actions artifact.

Only install this test package on a jailbroken console you control. If it launches to a blank screen, that is expected for this boot-only milestone; report whether it launches, remains running, and exits cleanly.
