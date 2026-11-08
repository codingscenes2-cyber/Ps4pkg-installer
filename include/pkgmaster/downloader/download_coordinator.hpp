#pragma once
#include "pkgmaster/core/queue.hpp"
#include "pkgmaster/downloader/parallel_range_downloader.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace pkgmaster {
struct CoordinatedDownloadResult {
    DownloadStatus status{DownloadStatus::Failed};
    bool used_parallel{false};
    std::string error;
};

// Synchronous coordinator intended to be called from a worker thread.
// pause/cancel/resume may be called from the UI thread.
class DownloadCoordinator {
public:
    explicit DownloadCoordinator(Queue& queue) : queue_(queue) {}
    CoordinatedDownloadResult run(std::uint64_t job_id,
                                  HttpDownloadOptions options,
                                  unsigned connections = 4,
                                  std::uint64_t minimum_part_bytes = 1024ull*1024ull);
    bool pause(std::uint64_t job_id);
    bool cancel(std::uint64_t job_id);
    bool resume(std::uint64_t job_id);

private:
    struct ActiveJob {
        std::shared_ptr<std::atomic_bool> stop;
        bool resumable{false};
    };
    Queue& queue_;
    std::mutex active_mutex_;
    std::unordered_map<std::uint64_t, ActiveJob> active_;
};
}
