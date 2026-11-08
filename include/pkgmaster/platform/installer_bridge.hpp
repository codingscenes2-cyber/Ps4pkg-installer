#pragma once
#include <string>

namespace pkgmaster {
enum class InstallHandoffResult { Accepted, Unsupported, Rejected, Error };

// Platform adapter contract only. Do not report success unless the real
// platform installer confirms that it accepted the package handoff.
class InstallerBridge {
public:
    virtual ~InstallerBridge() = default;
    virtual InstallHandoffResult submit(const std::string& absolute_pkg_path,
                                        std::string& message) = 0;
};

class UnavailableInstallerBridge final : public InstallerBridge {
public:
    InstallHandoffResult submit(const std::string&, std::string& message) override;
};
}
