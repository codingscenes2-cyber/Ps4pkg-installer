#include "pkgmaster/downloader/download_metrics.hpp"
#include <cassert>
#include <chrono>
int main() {
    using Clock = pkgmaster::DownloadSpeedTracker::Clock;
    const auto start = Clock::time_point{};
    pkgmaster::DownloadSpeedTracker tracker;
    auto first = tracker.update(0, 1000, start);
    assert(first.percent == 0.0 && !first.eta_available);
    auto second = tracker.update(500, 1000, start + std::chrono::seconds(2));
    assert(second.percent == 50.0);
    assert(second.bytes_per_second == 250.0);
    assert(second.eta_available && second.eta_seconds == 2);
    auto done = tracker.update(1000, 1000, start + std::chrono::seconds(4));
    assert(done.percent == 100.0 && !done.eta_available);
    auto reset = tracker.update(100, 1000, start + std::chrono::seconds(5));
    assert(reset.bytes_per_second == 0.0);
    auto zero_total = tracker.update(100, 0, start + std::chrono::seconds(6));
    assert(zero_total.percent == 0.0 && !zero_total.eta_available);
    return 0;
}
