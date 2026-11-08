#pragma once
#include <chrono>
#include <cstdint>
#include <algorithm>

namespace pkgmaster {
struct DownloadMetrics {
    std::uint64_t received_bytes{};
    std::uint64_t total_bytes{};
    double bytes_per_second{};
    std::uint64_t eta_seconds{};
    double percent{};
    bool eta_available{false};
};

class DownloadSpeedTracker {
public:
    using Clock = std::chrono::steady_clock;
    DownloadMetrics update(std::uint64_t received, std::uint64_t total,
                           Clock::time_point now = Clock::now()) {
        if (!started_ || received < last_received_) {
            started_ = true;
            last_received_ = received;
            last_time_ = now;
            speed_ = 0.0;
        } else {
            const double seconds = std::chrono::duration<double>(now-last_time_).count();
            if (seconds > 0.0 && received > last_received_) {
                const double sample = static_cast<double>(received-last_received_)/seconds;
                speed_ = speed_ == 0.0 ? sample : (0.25*sample + 0.75*speed_);
            }
            if (seconds > 0.0) { last_received_ = received; last_time_ = now; }
        }
        DownloadMetrics result;
        result.received_bytes = received;
        result.total_bytes = total;
        result.bytes_per_second = speed_;
        result.percent = total ? std::min(100.0, 100.0*static_cast<double>(received)/static_cast<double>(total)) : 0.0;
        if (total > received && speed_ > 0.0) {
            result.eta_seconds = static_cast<std::uint64_t>(
                (static_cast<double>(total-received)/speed_) + 0.999999);
            result.eta_available = true;
        }
        return result;
    }
private:
    bool started_{false};
    std::uint64_t last_received_{};
    Clock::time_point last_time_{};
    double speed_{};
};
}
