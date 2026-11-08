#pragma once
#include "pkgmaster/core/queue.hpp"
#include "pkgmaster/downloader/download_coordinator.hpp"
#include "pkgmaster/platform/phone_lan_server.hpp"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace pkgmaster {
// Concrete API adapter for the authenticated PhoneLanServer.
// Download destinations are always derived beneath the configured app directory;
// clients can submit URLs but cannot choose filesystem paths.
class PhoneApiController {
public:
    PhoneApiController(Queue& queue, DownloadCoordinator& downloads,
                       std::string downloads_directory,
                       std::string library_index_path,
                       bool run_download_worker = true);
    ~PhoneApiController();
    PhoneApiController(const PhoneApiController&) = delete;
    PhoneApiController& operator=(const PhoneApiController&) = delete;

    bool handle(const PhoneHttpRequest& request, PhoneHttpResponse& response);
    PhoneApiHandler handler() { return [this](const PhoneHttpRequest& r, PhoneHttpResponse& s) { return handle(r, s); }; }

private:
    void enqueue(std::uint64_t id);
    void worker_loop();
    Queue& queue_;
    DownloadCoordinator& downloads_;
    std::string downloads_directory_;
    std::string library_index_path_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<std::uint64_t> pending_;
    std::uint64_t active_id_{};
    bool stopping_{};
    std::thread worker_;
};
}
