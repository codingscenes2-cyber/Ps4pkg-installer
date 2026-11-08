#pragma once
#include "pkgmaster/downloader/http_downloader.hpp"
#include <cstdint>

namespace pkgmaster {
struct ParallelRangeOptions {
    HttpDownloadOptions transfer;
    unsigned connections{4};
    std::uint64_t minimum_part_bytes{1024ull * 1024ull};
};

struct ParallelRangeResult {
    DownloadStatus status{DownloadStatus::Failed};
    std::uint64_t received_bytes{};
    std::uint64_t expected_bytes{};
    unsigned completed_parts{};
    std::string error;
};

// Downloads a stable, strongly-ETagged HTTP resource using bounded concurrent
// range requests. Falls back are intentionally left to the caller.
ParallelRangeResult download_http_ranges(const ParallelRangeOptions& options);
}
