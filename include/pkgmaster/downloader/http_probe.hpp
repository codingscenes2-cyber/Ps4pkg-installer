#pragma once
#include <cstdint>
#include <string>

namespace pkgmaster {
struct HttpResourceProbe {
    bool reachable{false};
    bool range_supported{false};
    bool resumable{false};
    long http_status{};
    std::uint64_t content_length{};
    std::string strong_etag;
    std::string error;
};

// Performs HEAD followed by a one-byte range validation. Redirects are rejected;
// callers must not treat advertised Accept-Ranges alone as proof of support.
HttpResourceProbe probe_http_resource(const std::string& url,
                                      long connect_timeout_ms = 15000);
}
