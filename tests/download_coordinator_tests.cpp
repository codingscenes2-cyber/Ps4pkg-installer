#include "pkgmaster/downloader/download_coordinator.hpp"
#include <cassert>
int main() {
    pkgmaster::Queue queue;
    pkgmaster::Job job;
    job.source_url = "file:///etc/passwd";
    job.local_path = "coordinator-must-not-create.pkg";
    const auto id = queue.add(job);
    pkgmaster::DownloadCoordinator coordinator(queue);
    pkgmaster::HttpDownloadOptions options;
    auto result = coordinator.run(id, options);
    assert(result.status == pkgmaster::DownloadStatus::Failed);
    auto stored = queue.get(id);
    assert(stored && stored->state == pkgmaster::JobState::Failed);
    assert(!stored->error.empty());
    assert(!coordinator.resume(id));
    assert(coordinator.cancel(999999));
    return 0;
}
