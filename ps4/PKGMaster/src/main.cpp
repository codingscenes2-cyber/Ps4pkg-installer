#include <cstdio>
#include <orbis/libkernel.h>

// First PS4-targeted vertical slice: prove the OpenOrbis CRT and libkernel
// link correctly, then remain alive so the user can verify the app launches.
// This deliberately does not claim to provide the PKG manager UI or installer.
int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::puts("PKGMaster PS4 boot test");
    std::puts("OpenOrbis entry point started successfully.");
    std::puts("Native UI, storage discovery and installer handoff are not enabled yet.");

    for (;;) {
        sceKernelUsleep(1000000);
    }

    return 0;
}
