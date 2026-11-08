#include "pkgmaster/platform/phone_service_runtime.hpp"
#include "pkgmaster/core/queue_store.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "pkgmaster-phone-runtime-persistence";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    const auto snapshot = (root / "queue.idx").string();
    pkgmaster::Job saved;
    saved.id = 17;
    saved.source_url = "https://example.test/game.pkg";
    saved.file_name = "game.pkg";
    saved.local_path = (root / "game.pkg").string();
    saved.state = pkgmaster::JobState::Downloading;
    saved.expected_bytes = 1000;
    saved.received_bytes = 400;
    std::string error;
    assert(pkgmaster::QueueStore::save(snapshot, {saved}, error));

    pkgmaster::Queue queue;
    pkgmaster::DownloadCoordinator downloads(queue);
    pkgmaster::PhoneServiceRuntime service(queue, downloads, (root / "downloads").string(),
                                           (root / "library.idx").string(), {}, snapshot);
    std::string url, qr;
    assert(!service.start("127.0.0.1", 8080, url, qr, error));
    const auto restored = queue.snapshot();
    assert(restored.size() == 1);
    assert(restored[0].id == 17);
    assert(restored[0].state == pkgmaster::JobState::Queued);
    assert(restored[0].received_bytes == 400);
    service.stop();
    assert(service.persistence_error().empty());

    pkgmaster::Queue bad_queue;
    pkgmaster::Job existing;
    existing.file_name = "keep.pkg";
    bad_queue.add(existing);
    pkgmaster::DownloadCoordinator bad_downloads(bad_queue);
    {
        std::ofstream out(snapshot, std::ios::binary | std::ios::trunc);
        out << "NOT_A_QUEUE\n";
    }
    pkgmaster::PhoneServiceRuntime bad_service(bad_queue, bad_downloads,
        (root / "downloads").string(), (root / "library.idx").string(), {}, snapshot);
    error.clear();
    assert(!bad_service.start("192.168.1.20", 8080, url, qr, error));
    assert(!error.empty());
    assert(bad_queue.snapshot().size() == 1);
    assert(bad_queue.snapshot()[0].file_name == "keep.pkg");
    fs::remove_all(root, ec);
}
