#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace pkgmaster {
enum class DownloadStatus { Completed, Cancelled, Failed, Unsupported };

struct HttpDownloadOptions {
    std::string url;
    std::string destination_path;
    std::uint64_t maximum_bytes{50ull * 1024ull * 1024ull * 1024ull};
    long connect_timeout_ms{15000};
    long total_timeout_seconds{0};
    bool overwrite{false};
    const std::atomic_bool* cancel{nullptr};
    std::function<void(std::uint64_t received, std::uint64_t total)> progress;
};

struct HttpDownloadResult {
    DownloadStatus status{DownloadStatus::Failed};
    long http_status{};
    std::uint64_t received_bytes{};
    std::uint64_t expected_bytes{};
    std::string error;
};

// Single-stream HTTP/HTTPS transfer. Redirects are deliberately rejected:
// each redirect hop must be revalidated before a later implementation follows it.
HttpDownloadResult download_http_file(const HttpDownloadOptions& options);
}
