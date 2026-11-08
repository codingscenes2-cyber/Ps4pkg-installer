#pragma once
#include <string>

namespace pkgmaster {
// Conservative first-pass URL validation. Network code must additionally
// resolve and re-check every redirect target before connecting.
bool is_allowed_download_url(const std::string& url, std::string& reason);
}
