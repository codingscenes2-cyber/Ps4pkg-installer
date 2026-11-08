#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace pkgmaster {
struct PackageMetadata {
    std::string title;
    std::string title_id;
    std::string app_version;
    std::string category;
    std::string system_version;
};
bool parse_param_sfo(const std::vector<std::uint8_t>& bytes,
                     PackageMetadata& output, std::string& error);
bool read_param_sfo(const std::string& path,
                    PackageMetadata& output, std::string& error);
}
