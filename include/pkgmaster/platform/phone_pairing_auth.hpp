#pragma once
#include <cstdint>
#include <string>

namespace pkgmaster {
// In-memory one-time pairing and bearer-session state for the future LAN service.
// The caller MUST supply independently generated 256-bit CSPRNG tokens.
class PhonePairingAuth {
public:
    // pairing_secret and session_token must each be 64 lowercase/uppercase hex
    // characters representing 32 bytes from an operating-system CSPRNG.
    // Pairing lifetime is capped at 5 minutes; session lifetime at 30 days.
    bool arm(const std::string& pairing_secret,
             const std::string& session_token,
             std::uint64_t now_ms,
             std::uint64_t pairing_lifetime_ms,
             std::uint64_t session_lifetime_ms,
             std::string& error);
    // A successful exchange consumes the pairing secret exactly once.
    bool exchange(const std::string& supplied_secret,
                  std::uint64_t now_ms,
                  std::string& issued_session_token);
    bool authenticate(const std::string& supplied_session_token,
                      std::uint64_t now_ms) const;
    void revoke();
    bool pairing_pending() const noexcept { return !pairing_secret_.empty(); }

private:
    static bool is_token(const std::string& value) noexcept;
    static bool constant_time_equal(const std::string& a,
                                    const std::string& b) noexcept;
    static void wipe(std::string& value) noexcept;
    std::string pairing_secret_;
    std::string session_token_;
    std::uint64_t pairing_expires_ms_{};
    std::uint64_t session_expires_ms_{};
    bool session_active_{};
};
}
