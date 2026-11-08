#include "pkgmaster/platform/phone_api_controller.hpp"
#include "pkgmaster/package/library_index_store.hpp"
#include <cassert>
#include <filesystem>
#include <string>

int main() {
    const auto root = std::filesystem::temp_directory_path() / "pkgmaster-phone-api-tests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    pkgmaster::Queue queue;
    pkgmaster::DownloadCoordinator coordinator(queue);
    {
        pkgmaster::PhoneApiController api(queue, coordinator, (root / "downloads").string(),
                                          (root / "library.idx").string(), false);
        pkgmaster::PhoneHttpRequest request;
        pkgmaster::PhoneHttpResponse response;
        request.method = "GET";
        request.target = "/api/v1/status";
        assert(api.handle(request, response));
        assert(response.status == 200 && response.body.find("PKGMaster") != std::string::npos);
        request.target = "/api/v1/downloads";
        assert(api.handle(request, response));
        assert(response.status == 200 && response.body == "{\"downloads\":[]}");
        request.method = "POST";
        request.target = "/api/v1/downloads";
        request.headers["content-type"] = "text/plain";
        request.body = "http://127.0.0.1/unsafe.pkg";
        assert(api.handle(request, response));
        assert(response.status == 400 && queue.snapshot().empty());
        request.body = "https://example.com/not-a-package.zip";
        assert(api.handle(request, response));
        assert(response.status == 400 && queue.snapshot().empty());
        request.body = "https://example.com/a.pkg";
        assert(api.handle(request, response));
        assert(response.status == 201);
        const auto jobs = queue.snapshot();
        assert(jobs.size() == 1 && jobs[0].state == pkgmaster::JobState::Queued);
        request.method = "GET";
        request.target = "/api/v1/downloads";
        assert(api.handle(request, response));
        assert(response.body.find("\"file_name\":\"a.pkg\"") != std::string::npos);
        assert(response.body.find("package_path") == std::string::npos);
        request.method = "POST";
        request.target = "/api/v1/downloads/1/pause";
        assert(api.handle(request, response) && response.status == 200);
        request.target = "/api/v1/downloads/1/resume";
        assert(api.handle(request, response) && response.status == 202);
        request.method = "DELETE";
        request.target = "/api/v1/downloads/1";
        assert(api.handle(request, response) && response.status == 200);
        assert(queue.get(1)->state == pkgmaster::JobState::Cancelled);
        pkgmaster::LibraryPackage item;
        item.metadata.title = "Test Game";
        item.metadata.title_id = "TEST00001";
        item.metadata.app_version = "01.00";
        item.metadata.category = "gd";
        std::string save_error;
        assert(pkgmaster::PackageLibraryIndexStore::save((root / "library.idx").string(), {item}, save_error));
        request.method = "GET";
        request.target = "/api/v1/library";
        assert(api.handle(request, response) && response.status == 200);
        assert(response.body.find("Test Game") != std::string::npos);
        assert(response.body.find("package_path") == std::string::npos);
    }
    std::filesystem::remove_all(root, ec);
}
