#include "pkgmaster/platform/phone_api_controller.hpp"
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
                                          (root / "library.idx").string());
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
        assert(queue.snapshot().size() == 1);
        request.method = "GET";
        request.target = "/api/v1/downloads";
        assert(api.handle(request, response));
        assert(response.body.find("\"file_name\":\"a.pkg\"") != std::string::npos);
    }
    std::filesystem::remove_all(root, ec);
}
