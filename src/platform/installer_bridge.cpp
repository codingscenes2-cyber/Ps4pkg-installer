#include "pkgmaster/platform/installer_bridge.hpp"

namespace pkgmaster {
InstallHandoffResult UnavailableInstallerBridge::submit(
    const std::string&, std::string& message) {
    message = "Native package-install handoff is not available in this build.";
    return InstallHandoffResult::Unsupported;
}
}
