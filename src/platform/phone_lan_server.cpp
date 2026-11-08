#include "pkgmaster/platform/phone_lan_server.hpp"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstring>
#include <cerrno>
#include <utility>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace pkgmaster {
namespace {
#if defined(_WIN32)
using Socket = SOCKET;
constexpr Socket kInvalidSocket = INVALID_SOCKET;
void close_socket(Socket s) { if (s != kInvalidSocket) closesocket(s); }
int socket_error() { return WSAGetLastError(); }
#else
using Socket = int;
constexpr Socket kInvalidSocket = -1;
void close_socket(Socket s) { if (s >= 0) ::close(s); }
int socket_error() { return errno; }
#endif
std::uint64_t monotonic_ms() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
std::string status_text(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 415: return "Unsupported Media Type";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 503: return "Service Unavailable";
        default: return "Error";
    }
}
bool send_all(Socket socket, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
#if defined(_WIN32)
        const int n = ::send(socket, data.data() + sent,
                             static_cast<int>(data.size() - sent), 0);
#else
        const auto n = ::send(socket, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
#endif
        if (n <= 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}
std::string lower(std::string value) {
    for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
bool content_length_from_header(const std::string& bytes, std::size_t& length) {
    const auto end = bytes.find("\r\n\r\n");
    if (end == std::string::npos) return false;
    std::size_t pos = bytes.find("\r\n") + 2;
    while (pos < end) {
        const auto line_end = bytes.find("\r\n", pos);
        if (line_end == std::string::npos || line_end > end) break;
        const auto line = bytes.substr(pos, line_end - pos);
        const auto colon = line.find(':');
        if (colon != std::string::npos && lower(line.substr(0, colon)) == "content-length") {
            std::string value = line.substr(colon + 1);
            const auto first = value.find_first_not_of(" \t");
            if (first == std::string::npos) return false;
            value.erase(0, first);
            const auto last = value.find_last_not_of(" \t");
            value.erase(last + 1);
            if (value.empty()) return false;
            std::size_t parsed = 0;
            for (char c : value) {
                if (c < '0' || c > '9' || parsed > (1024u * 1024u - static_cast<unsigned>(c - '0')) / 10u)
                    return false;
                parsed = parsed * 10 + static_cast<unsigned>(c - '0');
            }
            length = parsed;
            return true;
        }
        pos = line_end + 2;
    }
    length = 0;
    return true;
}
bool receive_request(Socket client, std::string& bytes) {
    constexpr std::size_t max_request = 128 * 1024;
    char buffer[4096];
    std::size_t expected_total = 0;
    bool have_header = false;
    while (bytes.size() < max_request) {
#if defined(_WIN32)
        const int count = ::recv(client, buffer, sizeof(buffer), 0);
#else
        const auto count = ::recv(client, buffer, sizeof(buffer), 0);
#endif
        if (count <= 0) return false;
        bytes.append(buffer, static_cast<std::size_t>(count));
        const auto separator = bytes.find("\r\n\r\n");
        if (separator != std::string::npos && !have_header) {
            std::size_t body_length = 0;
            if (!content_length_from_header(bytes, body_length) || body_length > 64 * 1024)
                return false;
            expected_total = separator + 4 + body_length;
            have_header = true;
        }
        if (have_header && bytes.size() >= expected_total)
            return bytes.size() == expected_total;
    }
    return false;
}
void send_response(Socket client, const PhoneHttpResponse& response) {
    const int status = response.status >= 200 && response.status <= 599 ? response.status : 500;
    std::ostringstream head;
    head << "HTTP/1.1 " << status << ' ' << status_text(status) << "\r\n"
         << "Content-Type: " << response.content_type << "\r\n"
         << "Content-Length: " << response.body.size() << "\r\n"
         << "Cache-Control: no-store\r\n"
         << "X-Content-Type-Options: nosniff\r\n"
         << "Connection: close\r\n\r\n";
    send_all(client, head.str() + response.body);
}
PhoneHttpResponse error_response(int status, const char* message) {
    PhoneHttpResponse response;
    response.status = status;
    response.body = std::string("{\"error\":\"") + message + "\"}";
    return response;
}
}
PhoneLanServer::~PhoneLanServer() { stop(); }

bool PhoneLanServer::is_private_lan_ipv4(const std::string& address) noexcept {
    in_addr parsed{};
    if (::inet_pton(AF_INET, address.c_str(), &parsed) != 1) return false;
    const std::uint32_t host = ntohl(parsed.s_addr);
    return (host >= 0x0A000000u && host <= 0x0AFFFFFFu) ||
           (host >= 0xAC100000u && host <= 0xAC1FFFFFu) ||
           (host >= 0xC0A80000u && host <= 0xC0A8FFFFu);
}

bool PhoneLanServer::start(const std::string& address, std::uint16_t port,
                           std::string& pairing_secret, std::string& error,
                           PhoneApiHandler handler) {
    error.clear();
    pairing_secret.clear();
    if (running_) { error = "Phone service is already running"; return false; }
    if (!is_private_lan_ipv4(address) || port == 0) {
        error = "Bind address must be a private LAN IPv4 address and port must be nonzero";
        return false;
    }
#if defined(_WIN32)
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        error = "Winsock initialization failed"; return false;
    }
#endif
    Socket socket_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd == kInvalidSocket) {
        error = "Could not create phone service socket";
#if defined(_WIN32)
        WSACleanup();
#endif
        return false;
    }
#if !defined(_WIN32)
    int reuse = 1;
    ::setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    timeval timeout{}; timeout.tv_sec = 2;
    ::setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#else
    DWORD timeout = 2000;
    ::setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO,
                 reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#endif
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_port = htons(port);
    if (::inet_pton(AF_INET, address.c_str(), &endpoint.sin_addr) != 1 ||
        ::bind(socket_fd, reinterpret_cast<sockaddr*>(&endpoint), sizeof(endpoint)) != 0 ||
        ::listen(socket_fd, 4) != 0) {
        error = "Could not bind/listen on the selected LAN address";
        close_socket(socket_fd);
#if defined(_WIN32)
        WSACleanup();
#endif
        return false;
    }
    if (!auth_.arm_random(monotonic_ms(), 5 * 60 * 1000, 24 * 60 * 60 * 1000,
                          pairing_secret, error)) {
        close_socket(socket_fd);
#if defined(_WIN32)
        WSACleanup();
#endif
        return false;
    }
    port_ = port;
    handler_ = std::move(handler);
    listen_socket_.store(static_cast<std::intptr_t>(socket_fd));
    running_.store(true);
    try {
        worker_ = std::thread(&PhoneLanServer::serve, this);
    } catch (...) {
        running_.store(false);
        listen_socket_.store(-1);
        close_socket(socket_fd);
        auth_.revoke();
        pairing_secret.clear();
        error = "Could not start phone service worker";
#if defined(_WIN32)
        WSACleanup();
#endif
        return false;
    }
    return true;
}

void PhoneLanServer::stop() {
    if (!running_.exchange(false)) {
        if (worker_.joinable()) worker_.join();
        return;
    }
    const std::intptr_t socket_fd = listen_socket_.exchange(-1);
    if (socket_fd >= 0) {
#if defined(_WIN32)
        ::shutdown(static_cast<SOCKET>(socket_fd), SD_BOTH);
#else
        ::shutdown(socket_fd, SHUT_RDWR);
#endif
        close_socket(static_cast<Socket>(socket_fd));
    }
    if (worker_.joinable()) worker_.join();
    auth_.revoke();
    handler_ = {};
    port_ = 0;
#if defined(_WIN32)
    WSACleanup();
#endif
}

void PhoneLanServer::serve() {
    while (running_) {
        const Socket fd = static_cast<Socket>(listen_socket_.load());
        if (fd < 0) break;
        sockaddr_in peer{};
#if defined(_WIN32)
        int peer_size = sizeof(peer);
        Socket client = ::accept(fd, reinterpret_cast<sockaddr*>(&peer), &peer_size);
#else
        socklen_t peer_size = sizeof(peer);
        Socket client = ::accept(fd, reinterpret_cast<sockaddr*>(&peer), &peer_size);
#endif
        if (client == kInvalidSocket) {
            if (!running_) break;
            continue;
        }
#if !defined(_WIN32)
        timeval timeout{}; timeout.tv_sec = 2;
        ::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        ::setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#else
        DWORD timeout = 2000;
        ::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        ::setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#endif
        handle_client(static_cast<std::intptr_t>(client));
        close_socket(client);
    }
}

void PhoneLanServer::handle_client(std::intptr_t client_fd) {
    const Socket client = static_cast<Socket>(client_fd);
    std::string bytes;
    if (!receive_request(client, bytes)) {
        send_response(client, error_response(400, "invalid_or_incomplete_request"));
        return;
    }
    PhoneHttpRequest request;
    std::string error;
    if (!parse_phone_http_request(bytes, {}, request, error)) {
        send_response(client, error_response(400, "invalid_request"));
        return;
    }
    const auto now = monotonic_ms();
    if (request.method == "POST" && request.target == "/api/v1/pair") {
        const auto type = request.headers.find("content-type");
        if (type == request.headers.end() || lower(type->second) != "text/plain") {
            send_response(client, error_response(415, "pairing_requires_text_plain"));
            return;
        }
        std::string token;
        if (!auth_.exchange(request.body, now, token)) {
            send_response(client, error_response(401, "pairing_code_invalid_or_expired"));
            return;
        }
        PhoneHttpResponse response;
        response.status = 200;
        response.body = "{\"session_token\":\"" + token + "\"}";
        send_response(client, response);
        std::fill(token.begin(), token.end(), '\0');
        return;
    }
    const auto auth_header = request.headers.find("authorization");
    if (auth_header == request.headers.end() ||
        auth_header->second.compare(0, 7, "Bearer ") != 0 ||
        !auth_.authenticate(auth_header->second.substr(7), now)) {
        send_response(client, error_response(401, "unauthorized"));
        return;
    }
    PhoneHttpResponse response;
    if (!handler_ || !handler_(request, response)) {
        send_response(client, error_response(404, "endpoint_not_implemented"));
        return;
    }
    send_response(client, response);
}
}
