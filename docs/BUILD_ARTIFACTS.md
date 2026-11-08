# Build artifacts and validation

## Host validation bundle

The **Host validation artifact** GitHub Actions workflow builds the portable C++ core,
runs the complete CTest suite, and uploads a Linux x86_64 desktop bundle containing
the demo, phone-service host, and test executables.

To obtain it:
1. Open the repository's **Actions** tab.
2. Select **Host validation artifact** and open the latest successful run.
3. Download **pkgmaster-host-validation-linux-x86_64** from Artifacts.
4. Extract the archive and read `BUILD-INFO.txt`.

The artifact expires after 14 days. Re-run the workflow with **Run workflow** when a
fresh bundle is needed.

## Local validation

Run:

```sh
sh scripts/verify-host-build.sh
```

Requirements: CMake 3.20+, a C++17 compiler, libcurl development headers, and
libarchive development headers. If either optional library is unavailable, the
corresponding feature is compiled as unsupported by the current CMake configuration.

## Important platform boundary

This bundle is for Linux x86_64 and cannot be installed on a PS4. It is a reproducible
host test deliverable, not a PS4 ELF, FSELF, or PKG. A PS4 release still requires a
verified Orbis/OpenOrbis toolchain, PS4-specific platform entry point and UI,
compatible dependency builds, package metadata, and a successful target build.
No firmware 14.00 or GoldHEN compatibility is implied by host CI.
