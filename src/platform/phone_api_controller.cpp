#include "pkgmaster/platform/phone_api_controller.hpp"
#include "pkgmaster/core/url_policy.hpp"
#include "pkgmaster/package/library_index_store.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace pkgmaster {
namespace {
std::string json_escape(const std::string& value) {
    std::ostringstream out;
    for (unsigned char c : value) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) out << "\\u" << std::hex << std::setw(4)
                                  << std::setfill('0') << static_cast<unsigned>(c) << std::dec;
                else out << static_cast<char>(c);
        }
    }
    return out.str();
}
const char* state_name(JobState state) {
    switch (state) {
        case JobState::Queued: return "queued";
        case JobState::Resolving: return "resolving";
        case JobState::Downloading: return "downloading";
        case JobState::Verifying: return "verifying";
        case JobState::ReadyToInstall: return "ready_to_install";
        case JobState::Installing: return "installing";
        case JobState::Paused: return "paused";
        case JobState::Completed: return "completed";
        case JobState::Failed: return "failed";
        case JobState::Cancelled: return "cancelled";
    }
    return "unknown";
}
bool parse_id(const std::string& value, std::uint64_t& id) {
    if (value.empty()) return false;
    std::uint64_t result = 0;
    for (char c : value) {
        if (c < '0' || c > '9') return false;
        const auto digit = static_cast<std::uint64_t>(c - '0');
        if (result > (UINT64_MAX - digit) / 10) return false;
        result = result * 10 + digit;
    }
    if (!result) return false;
    id = result;
    return true;
}
bool package_filename(const std::string& url, std::string& filename) {
    const auto scheme = url.find("://");
    if (scheme == std::string::npos) return false;
    const auto path_start = url.find('/', scheme + 3);
    if (path_start == std::string::npos) return false;
    const auto path_end = url.find_first_of("?#", path_start);
    const std::string path = url.substr(path_start, path_end == std::string::npos ? std::string::npos : path_end - path_start);
    const auto slash = path.find_last_of('/');
    if (slash == std::string::npos || slash + 1 == path.size()) return false;
    std::string raw = path.substr(slash + 1);
    if (raw.size() > 180) return false;
    filename.reserve(raw.size());
    for (unsigned char c : raw) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-') filename.push_back(static_cast<char>(c));
        else filename.push_back('_');
    }
    if (filename.empty() || filename == "." || filename == ".." || filename.size() < 5) return false;
    std::string extension = filename.substr(filename.size() - 4);
    for (char& c : extension) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return extension == ".pkg";
}
PhoneHttpResponse json_response(int status, const std::string& body) {
    PhoneHttpResponse response;
    response.status = status;
    response.content_type = "application/json; charset=utf-8";
    response.body = body;
    return response;
}
}
PhoneApiController::PhoneApiController(Queue& queue, DownloadCoordinator& downloads,
                                       std::string downloads_directory,
                                       std::string library_index_path,
                                       bool run_download_worker)
    : queue_(queue), downloads_(downloads),
      downloads_directory_(std::move(downloads_directory)),
      library_index_path_(std::move(library_index_path)) {
    if (run_download_worker) {
        worker_ = std::thread(&PhoneApiController::worker_loop, this);
        for (const auto& job : queue_.snapshot())
            if (job.state == JobState::Queued || job.state == JobState::Resolving ||
                job.state == JobState::Downloading) enqueue(job.id);
    }
}
PhoneApiController::~PhoneApiController() {
    std::uint64_t active = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        active = active_id_;
        pending_.clear();
    }
    if (active) downloads_.cancel(active);
    condition_.notify_all();
    if (worker_.joinable()) worker_.join();
}
void PhoneApiController::enqueue(std::uint64_t id) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return;
        if (std::find(pending_.begin(), pending_.end(), id) != pending_.end()) return;
        pending_.push_back(id);
    }
    condition_.notify_one();
}
void PhoneApiController::worker_loop() {
    for (;;) {
        std::uint64_t id = 0;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [&] { return stopping_ || !pending_.empty(); });
            if (stopping_) return;
            id = pending_.front();
            pending_.pop_front();
            active_id_ = id;
        }
        const auto job = queue_.get(id);
        if (job && (job->state == JobState::Queued || job->state == JobState::Resolving ||
                    job->state == JobState::Downloading)) {
            HttpDownloadOptions options;
            options.maximum_bytes = 50ull * 1024ull * 1024ull * 1024ull;
            options.connect_timeout_ms = 15000;
            downloads_.run(id, std::move(options));
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (active_id_ == id) active_id_ = 0;
        }
    }
}
bool PhoneApiController::handle(const PhoneHttpRequest& request, PhoneHttpResponse& response) {
    const std::string& path = request.target;
    if (request.method == "GET" && path == "/api/v1/status") {
        response = json_response(200, "{\"service\":\"PKGMaster\",\"api_version\":1,\"native_install_handoff\":false}");
        return true;
    }
    if (request.method == "GET" && path == "/api/v1/downloads") {
        std::ostringstream body;
        body << "{\"downloads\":[";
        const auto jobs = queue_.snapshot();
        for (std::size_t i = 0; i < jobs.size(); ++i) {
            if (i) body << ',';
            const auto& job = jobs[i];
            body << "{\"id\":" << job.id
                 << ",\"file_name\":\"" << json_escape(job.file_name)
                 << "\",\"state\":\"" << state_name(job.state)
                 << "\",\"received_bytes\":" << job.received_bytes
                 << ",\"expected_bytes\":" << job.expected_bytes
                 << ",\"progress\":" << std::fixed << std::setprecision(4) << job.progress()
                 << ",\"error\":\"" << json_escape(job.error) << "\"}";
        }
        body << "]}";
        response = json_response(200, body.str());
        return true;
    }
    if (request.method == "POST" && path == "/api/v1/downloads") {
        const auto type = request.headers.find("content-type");
        if (type == request.headers.end() || type->second != "text/plain") {
            response = json_response(415, "{\"error\":\"content_type_must_be_text_plain\"}");
            return true;
        }
        std::string reason;
        if (!is_allowed_download_url(request.body, reason)) {
            response = json_response(400, "{\"error\":\"download_url_rejected\"}");
            return true;
        }
        std::string filename;
        if (!package_filename(request.body, filename)) {
            response = json_response(400, "{\"error\":\"url_must_end_in_pkg_filename\"}");
            return true;
        }
        std::error_code ec;
        std::filesystem::create_directories(downloads_directory_, ec);
        if (ec) {
            response = json_response(500, "{\"error\":\"download_directory_unavailable\"}");
            return true;
        }
        Job job;
        job.source_url = request.body;
        job.file_name = filename;
        const auto id = queue_.add(job);
        const auto destination = std::filesystem::path(downloads_directory_) /
                                 (std::to_string(id) + "-" + filename);
        job = *queue_.get(id);
        job.local_path = destination.string();
        if (!queue_.update(job)) {
            queue_.cancel(id);
            response = json_response(500, "{\"error\":\"could_not_store_download_job\"}");
            return true;
        }
        enqueue(id);
        response = json_response(201, "{\"id\":" + std::to_string(id) +
                                  ",\"state\":\"queued\",\"file_name\":\"" +
                                  json_escape(filename) + "\"}");
        return true;
    }
    const std::string prefix = "/api/v1/downloads/";
    if (path.compare(0, prefix.size(), prefix) == 0) {
        const auto slash = path.find('/', prefix.size());
        const std::string id_text = path.substr(prefix.size(), slash == std::string::npos ? std::string::npos : slash - prefix.size());
        std::uint64_t id = 0;
        if (!parse_id(id_text, id)) {
            response = json_response(400, "{\"error\":\"invalid_download_id\"}");
            return true;
        }
        const std::string action = slash == std::string::npos ? "" : path.substr(slash);
        if (request.method == "POST" && action == "/pause") {
            auto job = queue_.get(id);
            bool ok = false;
            if (job && job->state == JobState::Queued) ok = queue_.pause(id);
            else ok = downloads_.pause(id);
            response = json_response(ok ? 200 : 409, ok ? "{\"state\":\"paused\"}" : "{\"error\":\"pause_not_available\"}");
            return true;
        }
        if (request.method == "POST" && action == "/resume") {
            const bool ok = downloads_.resume(id);
            if (ok) enqueue(id);
            response = json_response(ok ? 202 : 409, ok ? "{\"state\":\"queued\"}" : "{\"error\":\"resume_not_available\"}");
            return true;
        }
        if (request.method == "POST" && action == "/retry") {
            const bool ok = queue_.retry(id);
            if (ok) enqueue(id);
            response = json_response(ok ? 202 : 409, ok ? "{\"state\":\"queued\"}" : "{\"error\":\"retry_not_available\"}");
            return true;
        }
        if (request.method == "DELETE" && action.empty()) {
            const bool ok = downloads_.cancel(id);
            response = json_response(ok ? 200 : 409, ok ? "{\"state\":\"cancelled\"}" : "{\"error\":\"cancel_not_available\"}");
            return true;
        }
    }
    if (request.method == "GET" && path == "/api/v1/library") {
        std::vector<LibraryPackage> packages;
        std::string error;
        std::error_code exists_error;
        const bool index_exists = std::filesystem::exists(library_index_path_, exists_error);
        if (exists_error || (index_exists && !PackageLibraryIndexStore::load(library_index_path_, packages, error))) {
            if (!error.empty()) {
                response = json_response(500, "{\"error\":\"library_index_unavailable\"}");
                return true;
            }
        }
        std::ostringstream body;
        body << "{\"packages\":[";
        for (std::size_t i = 0; i < packages.size(); ++i) {
            if (i) body << ',';
            const auto& item = packages[i];
            body << "{\"title\":\"" << json_escape(item.metadata.title)
                 << "\",\"title_id\":\"" << json_escape(item.metadata.title_id)
                 << "\",\"app_version\":\"" << json_escape(item.metadata.app_version)
                 << "\",\"category\":\"" << json_escape(item.metadata.category)
                 << "\",\"system_version\":\"" << json_escape(item.metadata.system_version)
                 << "\",\"package_path\":\"" << json_escape(item.package_path) << "\"}";
        }
        body << "]}";
        response = json_response(200, body.str());
        return true;
    }
    if (request.method == "POST" && path.compare(0, 20, "/api/v1/installs/") == 0) {
        response = json_response(501, "{\"error\":\"native_install_handoff_not_available\"}");
        return true;
    }
    response = json_response(404, "{\"error\":\"endpoint_not_found\"}");
    return true;
}
}
