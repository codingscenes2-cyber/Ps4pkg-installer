#pragma once
#include "pkgmaster/platform/phone_http_parser.hpp"
#include "pkgmaster/platform/phone_pairing_auth.hpp"
#include "pkgmaster/platform/phone_pairing_rate_limit.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <cstdint>
#include <string>
#include <thread>

namespace pkgmaster {
struct PhoneHttpResponse {
    int status{200};
    std::string content_type{"application/json"};
    std::string body{"{}"};
};
using PhoneApiHandler = std::function<bool(const PhoneHttpRequest&, PhoneHttpResponse&)>;
// Requires the exact listener Host header and, when present, same-origin HTTP Origin.
bool is_allowed_phone_request_host_origin(const PhoneHttpRequest& request,
                                          const std::string& expected_host);

// Explicitly opt-in, single-client-at-a-time LAN HTTP service.
// Binds only to an RFC1918 IPv4 address supplied by the caller.
class PhoneLanServer {
public:
    PhoneLanServer() = default;
    ~PhoneLanServer();
    PhoneLanServer(const PhoneLanServer&) = delete;
    PhoneLanServer& operator=(const PhoneLanServer&) = delete;

    bool start(const std::string& private_lan_ipv4, std::uint16_t port,
               std::string& pairing_secret, std::string& error,
               PhoneApiHandler handler = {});
    void stop();
    bool running() const noexcept { return running_.load(); }
    std::uint16_t port() const noexcept { return port_; }
    static bool is_private_lan_ipv4(const std::string& address) noexcept;

private:
    void serve();
    void handle_client(std::intptr_t client);
    std::atomic_bool running_{false};
    std::atomic<std::intptr_t> listen_socket_{-1};
    std::thread worker_;
    std::uint16_t port_{};
    std::string expected_host_;
    PhonePairingAuth auth_;
    PhonePairingRateLimiter pairing_limiter_;
    PhoneApiHandler handler_;
};
}
