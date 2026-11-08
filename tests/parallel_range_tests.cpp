#include "pkgmaster/downloader/parallel_range_downloader.hpp"
#include <cassert>
#include <limits>
int main() {
    pkgmaster::ParallelRangeOptions options;
    options.transfer.url = "file:///etc/passwd";
    options.transfer.destination_path = "parallel-test.pkg";
    auto invalid = pkgmaster::download_http_ranges(options);
    assert(invalid.status == pkgmaster::DownloadStatus::Failed);
    assert(!invalid.error.empty());
    options.transfer.url = "https://127.0.0.1/private.pkg";
    auto private_host = pkgmaster::download_http_ranges(options);
    assert(private_host.status == pkgmaster::DownloadStatus::Failed);
    options.transfer.url = "https://example.com/file.pkg";
    options.connections = 1;
    auto bad_workers = pkgmaster::download_http_ranges(options);
    assert(bad_workers.status == pkgmaster::DownloadStatus::Failed);
    options.connections = 4;
    options.transfer.maximum_bytes_per_second = std::numeric_limits<std::uint64_t>::max();
    auto bad_limit = pkgmaster::download_http_ranges(options);
    assert(bad_limit.status == pkgmaster::DownloadStatus::Failed);
    return 0;
}
