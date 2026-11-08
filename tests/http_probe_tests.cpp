#include "pkgmaster/downloader/http_probe.hpp"
#include <cassert>
int main() {
    auto invalid = pkgmaster::probe_http_resource("file:///etc/passwd");
    assert(!invalid.reachable);
    assert(!invalid.range_supported);
    assert(!invalid.error.empty());
    auto private_host = pkgmaster::probe_http_resource("https://127.0.0.1/object.pkg");
    assert(!private_host.reachable);
    return 0;
}
