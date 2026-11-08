#include "pkgmaster/platform/phone_service_runtime.hpp"
#include <cassert>
#include <string>
int main() {
    pkgmaster::Queue queue;
    pkgmaster::DownloadCoordinator downloads(queue);
    pkgmaster::PhoneServiceRuntime service(queue, downloads, "/tmp/pkgmaster-runtime-downloads",
                                           "/tmp/pkgmaster-runtime-library.idx");
    std::string url, qr, error;
    assert(!service.running());
    assert(service.port() == 0);
    assert(!service.start("127.0.0.1", 8080, url, qr, error));
    assert(!service.running());
    assert(url.empty() && qr.empty());
    assert(!error.empty());
    assert(service.port() == 0);
    service.stop();
    assert(!service.running());
    // A missing app-owned path is rejected before any listener is opened.
    pkgmaster::PhoneServiceRuntime invalid(queue, downloads, "", "", {});
    error.clear();
    assert(!invalid.start("192.168.1.20", 8080, url, qr, error));
    assert(!error.empty());
    assert(!invalid.running());
}
