#pragma once
#include "pkgmaster/package/param_sfo.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pkgmaster {
struct LibraryPackage {
    PackageMetadata metadata;
    std::string package_path;
    std::string param_sfo_path;
    std::uint64_t param_sfo_bytes{};
};
struct PackageScanOptions {
    std::size_t max_depth{8};
    std::size_t max_directories{10000};
    std::size_t max_results{2000};
    bool accept_root_param_sfo{true};
};
// Scans extracted package/title directories only. It does not inspect .pkg archives.
bool scan_package_library(const std::vector<std::string>& roots,
                          const PackageScanOptions& options,
                          std::vector<LibraryPackage>& output,
                          std::string& error);
}
