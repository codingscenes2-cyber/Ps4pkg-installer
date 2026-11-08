#include "pkgmaster/platform/phone_service_runtime.hpp"
#include "pkgmaster/core/queue_store.hpp"
#include "pkgmaster/platform/phone_api_controller.hpp"
#include "pkgmaster/platform/phone_qr_code.hpp"
#include "pkgmaster/platform/phone_web_ui.hpp"
#include <utility>
#include <filesystem>

namespace pkgmaster {
PhoneServiceRuntime::PhoneServiceRuntime(
    Queue& queue, DownloadCoordinator& downloads,
    std::string downloads_directory, std::string library_index_path,
    std::vector<StorageCandidate> storage_candidates,
    std::string queue_snapshot_path)
    : queue_(queue), downloads_(downloads),
      downloads_directory_(std::move(downloads_directory)),
      library_index_path_(std::move(library_index_path)),
      queue_snapshot_path_(std::move(queue_snapshot_path)),
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
    if (!PhoneLanServer::is_private_lan_ipv4(address) || port == 0) {
        error = "Phone service requires a private LAN IPv4 address and nonzero port";
        return false;
    }
    if (downloads_directory_.empty() || library_index_path_.empty()) {
        error = "Phone service requires configured app-owned download and library paths";
        return false;
    }
    if (!queue_snapshot_path_.empty()) {
        std::error_code exists_error;
        const bool exists = std::filesystem::exists(queue_snapshot_path_, exists_error);
        if (exists_error) {
            error = "Could not inspect the saved download queue";
            return false;
        }
        if (exists) {
            if (!queue_.snapshot().empty()) {
                error = "Cannot restore a saved queue over an already populated queue";
                return false;
            }
            std::vector<Job> restored;
            if (!QueueStore::load(queue_snapshot_path_, restored, error)) {
                error = "Saved download queue is invalid: " + error;
                return false;
            }
            for (auto& job : restored) {
                if (job.state == JobState::Resolving || job.state == JobState::Downloading ||
                    job.state == JobState::Verifying) {
                    job.state = JobState::Queued;
                } else if (job.state == JobState::Installing) {
                    job.state = (job.expected_bytes && job.received_bytes == job.expected_bytes)
                                    ? JobState::ReadyToInstall : JobState::Queued;
                }
            }
            if (!queue_.restore(restored)) {
                error = "Could not restore the validated download queue";
                return false;
            }
        }
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
    persistence_error_.clear();
    if (!queue_snapshot_path_.empty()) {
        QueueStore::save(queue_snapshot_path_, queue_.snapshot(), persistence_error_);
    }
}

bool PhoneServiceRuntime::running() const noexcept {
    return server_ && server_->running();
}

std::uint16_t PhoneServiceRuntime::port() const noexcept {
    return server_ ? server_->port() : 0;
}
}
