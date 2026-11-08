#include "pkgmaster/downloader/http_downloader.hpp"
#include <atomic>
#include <cassert>
#include <cstdio>
int main() {
    pkgmaster::HttpDownloadOptions options;
    options.url = "file:///etc/passwd";
    options.destination_path = "must-not-be-created.pkg";
    auto rejected = pkgmaster::download_http_file(options);
    assert(rejected.status == pkgmaster::DownloadStatus::Failed);
    assert(!rejected.error.empty());
    options.url = "https://127.0.0.1/private.pkg";
    auto private_host = pkgmaster::download_http_file(options);
    assert(private_host.status == pkgmaster::DownloadStatus::Failed);
    std::atomic_bool cancelled{true};
    options.url = "https://example.com/test.pkg";
    options.cancel = &cancelled;
    auto cancelled_result = pkgmaster::download_http_file(options);
    assert(cancelled_result.status == pkgmaster::DownloadStatus::Cancelled ||
           cancelled_result.status == pkgmaster::DownloadStatus::Unsupported);
    std::remove("must-not-be-created.pkg");
    std::remove("must-not-be-created.pkg.part");
    return 0;
}
