#pragma once
#include "pkgmaster/core/queue.hpp"
#include "pkgmaster/downloader/download_coordinator.hpp"
#include "pkgmaster/platform/phone_lan_server.hpp"
#include "pkgmaster/platform/storage.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace pkgmaster {
class PhoneApiController;
// Owns the complete phone-service lifetime. The caller supplies a verified
// private LAN interface and app-owned storage paths; no listener starts by
// default. stop() always stops the socket worker before destroying its handler.
class PhoneServiceRuntime {
public:
    PhoneServiceRuntime(Queue& queue, DownloadCoordinator& downloads,
                        std::string downloads_directory,
                        std::string library_index_path,
                        std::vector<StorageCandidate> storage_candidates = {});
    ~PhoneServiceRuntime();
    PhoneServiceRuntime(const PhoneServiceRuntime&) = delete;
    PhoneServiceRuntime& operator=(const PhoneServiceRuntime&) = delete;

    bool start(const std::string& private_lan_ipv4, std::uint16_t port,
               std::string& pairing_url, std::string& pairing_qr_svg,
               std::string& error);
    void stop();
    bool running() const noexcept;
    std::uint16_t port() const noexcept;

private:
    Queue& queue_;
    DownloadCoordinator& downloads_;
    std::string downloads_directory_;
    std::string library_index_path_;
    std::vector<StorageCandidate> storage_candidates_;
    std::unique_ptr<PhoneApiController> controller_;
    std::unique_ptr<PhoneLanServer> server_;
};
}
