#include "pkgmaster/platform/phone_service_runtime.hpp"
#include "pkgmaster/platform/phone_api_controller.hpp"
#include "pkgmaster/platform/phone_qr_code.hpp"
#include "pkgmaster/platform/phone_web_ui.hpp"
#include <utility>

namespace pkgmaster {
PhoneServiceRuntime::PhoneServiceRuntime(
    Queue& queue, DownloadCoordinator& downloads,
    std::string downloads_directory, std::string library_index_path,
    std::vector<StorageCandidate> storage_candidates)
    : queue_(queue), downloads_(downloads),
      downloads_directory_(std::move(downloads_directory)),
      library_index_path_(std::move(library_index_path)),
      storage_candidates_(std::move(storage_candidates)) {}

PhoneServiceRuntime::~PhoneServiceRuntime() { stop(); }

bool PhoneServiceRuntime::start(const std::string& address, std::uint16_t port,
                                std::string& pairing_url,
                                std::string& pairing_qr_svg,
                                std::string& error) {
    pairing_url.clear();
    pairing_qr_svg.clear();
    error.clear();
    if (running()) {
        error = "Phone service is already running";
        return false;
    }
    if (downloads_directory_.empty() || library_index_path_.empty()) {
        error = "Phone service requires configured app-owned download and library paths";
        return false;
    }
    auto controller = std::make_unique<PhoneApiController>(
        queue_, downloads_, downloads_directory_, library_index_path_, true,
        storage_candidates_);
    auto server = std::make_unique<PhoneLanServer>();
    std::string secret;
    if (!server->start(address, port, secret, error, controller->handler())) {
        return false;
    }
    std::string url;
    if (!make_phone_pairing_url(address, server->port(), secret, url, error)) {
        server->stop();
        return false;
    }
    std::string svg;
    if (!generate_phone_pairing_qr_svg(url, svg, error)) {
        server->stop();
        return false;
    }
    // Publish owned objects only after every startup step has succeeded.
    pairing_url = std::move(url);
    pairing_qr_svg = std::move(svg);
    server_ = std::move(server);
    controller_ = std::move(controller);
    return true;
}

void PhoneServiceRuntime::stop() {
    if (server_) {
        server_->stop();
        server_.reset();
    }
    controller_.reset();
}

bool PhoneServiceRuntime::running() const noexcept {
    return server_ && server_->running();
}

std::uint16_t PhoneServiceRuntime::port() const noexcept {
    return server_ ? server_->port() : 0;
}
}
