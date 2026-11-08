#pragma once
#include "pkgmaster/platform/phone_http_parser.hpp"
#include "pkgmaster/platform/phone_pairing_auth.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

namespace pkgmaster {
struct PhoneHttpResponse {
    int status{200};
    std::string content_type{"application/json"};
    std::string body{"{}"};
};
using PhoneApiHandler = std::function<bool(const PhoneHttpRequest&, PhoneHttpResponse&)>;

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
    void handle_client(int client);
    std::atomic_bool running_{false};
    std::atomic<int> listen_socket_{-1};
    std::thread worker_;
    std::uint16_t port_{};
    PhonePairingAuth auth_;
    PhoneApiHandler handler_;
};
}
