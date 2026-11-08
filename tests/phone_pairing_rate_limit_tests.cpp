#include "pkgmaster/platform/phone_pairing_rate_limit.hpp"
#include <cassert>
#include <cstdint>
#include <limits>
int main() {
    pkgmaster::PhonePairingRateLimiter limiter;
    for (std::uint64_t i = 0; i < 4; ++i) {
        assert(!limiter.blocked(1000));
        limiter.record_failure(1000);
    }
    assert(!limiter.blocked(1000));
    limiter.record_failure(1000);
    assert(limiter.blocked(1000));
    assert(limiter.blocked(30999));
    assert(!limiter.blocked(31000));
    for (int i = 0; i < 5; ++i) limiter.record_failure(31000);
    assert(limiter.blocked(31001));
    limiter.record_success();
    assert(!limiter.blocked(31001));
    for (int i = 0; i < 5; ++i) limiter.record_failure(std::numeric_limits<std::uint64_t>::max() - 100);
    assert(limiter.blocked(std::numeric_limits<std::uint64_t>::max() - 50));
    limiter.reset();
    assert(!limiter.blocked(std::numeric_limits<std::uint64_t>::max() - 50));
}
