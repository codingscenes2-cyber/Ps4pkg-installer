#include "pkgmaster/platform/phone_pairing_auth.hpp"
#include <limits>

namespace pkgmaster {
namespace {
constexpr std::uint64_t kMinimumPairingLifetimeMs = 1000;
constexpr std::uint64_t kMaximumPairingLifetimeMs = 5ull * 60ull * 1000ull;
constexpr std::uint64_t kMinimumSessionLifetimeMs = 60ull * 1000ull;
constexpr std::uint64_t kMaximumSessionLifetimeMs = 30ull * 24ull * 60ull * 60ull * 1000ull;
bool is_hex(char c) noexcept {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}
}
bool PhonePairingAuth::is_token(const std::string& value) noexcept {
    if (value.size() != 64) return false;
    for (char c : value) if (!is_hex(c)) return false;
    return true;
}
bool PhonePairingAuth::constant_time_equal(const std::string& a,
                                           const std::string& b) noexcept {
    // Iterate over the maximum length so a mismatch does not early-exit.
    const std::size_t n = a.size() > b.size() ? a.size() : b.size();
    unsigned difference = static_cast<unsigned>(a.size() ^ b.size());
    for (std::size_t i = 0; i < n; ++i) {
        const unsigned char left = i < a.size() ? static_cast<unsigned char>(a[i]) : 0;
        const unsigned char right = i < b.size() ? static_cast<unsigned char>(b[i]) : 0;
        difference |= static_cast<unsigned>(left ^ right);
    }
    return difference == 0;
}
void PhonePairingAuth::wipe(std::string& value) noexcept {
    volatile char* data = value.empty() ? nullptr : &value[0];
    for (std::size_t i = 0; i < value.size(); ++i) data[i] = 0;
    value.clear();
}
bool PhonePairingAuth::arm(const std::string& pairing_secret,
                           const std::string& session_token,
                           std::uint64_t now_ms,
                           std::uint64_t pairing_lifetime_ms,
                           std::uint64_t session_lifetime_ms,
                           std::string& error) {
    revoke();
    error.clear();
    if (!is_token(pairing_secret) || !is_token(session_token)) {
        error = "Pairing and session credentials must be 256-bit hexadecimal tokens";
        return false;
    }
    if (pairing_lifetime_ms < kMinimumPairingLifetimeMs ||
        pairing_lifetime_ms > kMaximumPairingLifetimeMs ||
        session_lifetime_ms < kMinimumSessionLifetimeMs ||
        session_lifetime_ms > kMaximumSessionLifetimeMs ||
        now_ms > std::numeric_limits<std::uint64_t>::max() - pairing_lifetime_ms ||
        now_ms > std::numeric_limits<std::uint64_t>::max() - session_lifetime_ms) {
        error = "Pairing or session lifetime is outside the permitted bounds";
        return false;
    }
    if (constant_time_equal(pairing_secret, session_token)) {
        error = "Pairing secret and session token must be distinct";
        return false;
    }
    pairing_secret_ = pairing_secret;
    session_token_ = session_token;
    pairing_expires_ms_ = now_ms + pairing_lifetime_ms;
    session_expires_ms_ = now_ms + session_lifetime_ms;
    return true;
}
bool PhonePairingAuth::exchange(const std::string& supplied_secret,
                                std::uint64_t now_ms,
                                std::string& issued_session_token) {
    issued_session_token.clear();
    if (pairing_secret_.empty() || now_ms >= pairing_expires_ms_) {
        wipe(pairing_secret_);
        wipe(session_token_);
        pairing_expires_ms_ = session_expires_ms_ = 0;
        return false;
    }
    if (!constant_time_equal(pairing_secret_, supplied_secret)) return false;
    wipe(pairing_secret_);
    pairing_expires_ms_ = 0;
    issued_session_token = session_token_;
    return true;
}
bool PhonePairingAuth::authenticate(const std::string& supplied_session_token,
                                    std::uint64_t now_ms) const {
    if (session_token_.empty() || now_ms >= session_expires_ms_) return false;
    return constant_time_equal(session_token_, supplied_session_token);
}
void PhonePairingAuth::revoke() {
    wipe(pairing_secret_);
    wipe(session_token_);
    pairing_expires_ms_ = 0;
    session_expires_ms_ = 0;
}
}
