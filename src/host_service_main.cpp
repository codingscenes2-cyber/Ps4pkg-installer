#include "pkgmaster/core/queue.hpp"
#include "pkgmaster/downloader/download_coordinator.hpp"
#include "pkgmaster/platform/phone_service_runtime.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
volatile std::sig_atomic_t stop_requested = 0;
void handle_signal(int) { stop_requested = 1; }

bool parse_port(const char* text, std::uint16_t& port) {
    if (!text || !*text) return false;
    unsigned value = 0;
    for (const char* p = text; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        const unsigned digit = static_cast<unsigned>(*p - '0');
        if (value > (65535u - digit) / 10u) return false;
        value = value * 10u + digit;
    }
    if (value == 0) return false;
    port = static_cast<std::uint16_t>(value);
    return true;
}
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: pkgmaster_service_host <private-lan-ipv4> <port> <data-directory>\n";
        return 2;
    }
    const std::string address = argv[1];
    std::uint16_t port = 0;
    if (!pkgmaster::PhoneLanServer::is_private_lan_ipv4(address) ||
        !parse_port(argv[2], port)) {
        std::cerr << "Bind address must be RFC1918 IPv4 and port must be 1-65535.\n";
        return 2;
    }

    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path root = fs::absolute(argv[3], ec);
    if (ec || root.empty()) {
        std::cerr << "Could not resolve the data directory.\n";
        return 2;
    }
    const fs::path downloads_path = root / "downloads";
    fs::create_directories(downloads_path, ec);
    if (ec) {
        std::cerr << "Could not create the data/downloads directory: " << ec.message() << '\n';
        return 1;
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    pkgmaster::Queue queue;
    pkgmaster::DownloadCoordinator downloads(queue);
    std::vector<pkgmaster::StorageCandidate> storage{
        {"internal", "Application data", root.string(), pkgmaster::StorageKind::Internal}
    };
    pkgmaster::PhoneServiceRuntime service(
        queue, downloads, downloads_path.string(),
        (root / "library.idx").string(), std::move(storage),
        (root / "queue.idx").string());

    std::string pairing_url, pairing_qr, error;
    if (!service.start(address, port, pairing_url, pairing_qr, error)) {
        std::cerr << "Could not start PKGMaster phone service: " << error << '\n';
        return 1;
    }

    const fs::path qr_path = root / "pairing.svg";
    {
        std::ofstream qr_file(qr_path, std::ios::binary | std::ios::trunc);
        if (!qr_file || !(qr_file << pairing_qr) || !qr_file.flush()) {
            std::cerr << "Could not write pairing QR SVG. Stopping service.\n";
            service.stop();
            return 1;
        }
    }

    std::cout << "PKGMaster phone service is running.\n"
              << "Open this URL on your phone (same trusted LAN): " << pairing_url << '\n'
              << "QR SVG: " << qr_path.string() << '\n'
              << "Downloads: " << downloads_path.string() << '\n'
              << "Press Ctrl+C to stop safely and save the queue.\n";

    while (!stop_requested && service.running())
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

    service.stop();
    if (!service.persistence_error().empty()) {
        std::cerr << "Queue snapshot could not be saved: "
                  << service.persistence_error() << '\n';
        return 1;
    }
    std::cout << "PKGMaster phone service stopped. Queue snapshot saved.\n";
    return 0;
}
