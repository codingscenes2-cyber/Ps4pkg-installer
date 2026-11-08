#pragma once
#include <cstdint>
#include <limits>

namespace pkgmaster {
// Small monotonic-clock limiter for the one-time phone pairing endpoint.
// Five failed secrets trigger a 30-second cooldown.
class PhonePairingRateLimiter {
public:
    bool blocked(std::uint64_t now_ms) const noexcept {
        return lockout_until_ms_ != 0 && now_ms < lockout_until_ms_;
    }
    void record_failure(std::uint64_t now_ms) noexcept {
        if (blocked(now_ms)) return;
        if (++failed_attempts_ >= kFailureLimit) {
            lockout_until_ms_ = now_ms > std::numeric_limits<std::uint64_t>::max() - kCooldownMs
                                    ? std::numeric_limits<std::uint64_t>::max()
                                    : now_ms + kCooldownMs;
            failed_attempts_ = 0;
        }
    }
    void record_success() noexcept {
        failed_attempts_ = 0;
        lockout_until_ms_ = 0;
    }
    void reset() noexcept { record_success(); }

private:
    static constexpr unsigned kFailureLimit = 5;
    static constexpr std::uint64_t kCooldownMs = 30'000;
    unsigned failed_attempts_{};
    std::uint64_t lockout_until_ms_{};
};
}
