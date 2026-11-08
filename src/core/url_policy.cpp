#include "pkgmaster/core/url_policy.hpp"
#include <algorithm>
#include <cctype>

namespace pkgmaster {
bool is_allowed_download_url(const std::string& url, std::string& reason) {
    const auto colon = url.find("://");
    if (colon == std::string::npos) {
        reason = "URL must use HTTP or HTTPS.";
        return false;
    }
    std::string scheme = url.substr(0, colon);
    std::transform(scheme.begin(), scheme.end(), scheme.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (scheme != "http" && scheme != "https") {
        reason = "Only HTTP and HTTPS links are accepted.";
        return false;
    }
    const auto host_start = colon + 3;
    const auto host_end = url.find_first_of("/:?#", host_start);
    const std::string host = url.substr(host_start, host_end - host_start);
    if (host.empty() || host.find('@') != std::string::npos) {
        reason = "URL host is missing or contains credentials.";
        return false;
    }
    std::string lowered = host;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowered == "localhost" || lowered == "127.0.0.1" ||
        lowered == "[::1]" || lowered.rfind("169.254.", 0) == 0 ||
        lowered.rfind("10.", 0) == 0 || lowered.rfind("192.168.", 0) == 0 ||
        lowered.rfind("172.16.", 0) == 0) {
        reason = "Local/private destinations are blocked by default.";
        return false;
    }
    reason.clear();
    return true;
}
}
