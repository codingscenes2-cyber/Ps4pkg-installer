#include "pkgmaster/package/library_scanner.hpp"
#include <algorithm>
#include <filesystem>
#include <set>
#include <system_error>

namespace pkgmaster {
namespace fs = std::filesystem;
namespace {
bool regular_file_no_symlink(const fs::path& path, std::uint64_t& size) {
    std::error_code ec;
    const auto status = fs::symlink_status(path, ec);
    if (ec || fs::is_symlink(status) || !fs::is_regular_file(status)) return false;
    const auto n = fs::file_size(path, ec);
    if (ec) return false;
    size = static_cast<std::uint64_t>(n);
    return true;
}
std::string normalized_path(const fs::path& p) {
    std::error_code ec;
    auto absolute = fs::absolute(p, ec);
    return (ec ? p : absolute).lexically_normal().string();
}
}
bool scan_package_library(const std::vector<std::string>& roots,
                          const PackageScanOptions& options,
                          std::vector<LibraryPackage>& output,
                          std::string& error) {
    error.clear();
    if (options.max_directories == 0 || options.max_results == 0 ||
        options.max_depth > 128 || options.max_directories > 1000000 ||
        options.max_results > 100000) {
        error = "invalid package scan limits";
        return false;
    }
    std::vector<LibraryPackage> found;
    std::set<std::string> seen;
    std::size_t visited = 0;
    for (const auto& root_text : roots) {
        if (root_text.empty()) continue;
        const fs::path root(root_text);
        std::error_code ec;
        const auto root_status = fs::symlink_status(root, ec);
        if (ec || fs::is_symlink(root_status) || !fs::is_directory(root_status)) continue;
        std::vector<std::pair<fs::path, std::size_t>> pending{{root, 0}};
        while (!pending.empty()) {
            auto current = std::move(pending.back());
            pending.pop_back();
            if (++visited > options.max_directories) {
                error = "package scan directory limit exceeded";
                return false;
            }
            const fs::path sfo = current.first / "sce_sys" / "param.sfo";
            std::uint64_t bytes = 0;
            fs::path selected = sfo;
            bool has_sfo = regular_file_no_symlink(sfo, bytes);
            if (!has_sfo && options.accept_root_param_sfo) {
                selected = current.first / "param.sfo";
                has_sfo = regular_file_no_symlink(selected, bytes);
            }
            if (has_sfo) {
                const std::string key = normalized_path(selected);
                if (seen.insert(key).second) {
                    PackageMetadata metadata;
                    std::string parse_error;
                    if (read_param_sfo(key, metadata, parse_error)) {
                        found.push_back({std::move(metadata), normalized_path(current.first),
                                         key, bytes});
                        if (found.size() > options.max_results) {
                            error = "package scan result limit exceeded";
                            return false;
                        }
                    }
                }
            }
            if (current.second >= options.max_depth) continue;
            std::vector<fs::path> children;
            fs::directory_iterator it(current.first, fs::directory_options::skip_permission_denied, ec), end;
            if (ec) { ec.clear(); continue; }
            for (; it != end; it.increment(ec)) {
                if (ec) { ec.clear(); continue; }
                std::error_code sec;
                const auto status = it->symlink_status(sec);
                if (!sec && fs::is_directory(status) && !fs::is_symlink(status))
                    children.push_back(it->path());
            }
            std::sort(children.begin(), children.end(), [](const fs::path& a, const fs::path& b) {
                return a.generic_string() > b.generic_string();
            });
            for (auto& child : children) pending.emplace_back(std::move(child), current.second + 1);
        }
    }
    std::sort(found.begin(), found.end(), [](const LibraryPackage& a, const LibraryPackage& b) {
        if (a.metadata.title_id != b.metadata.title_id)
            return a.metadata.title_id < b.metadata.title_id;
        return a.package_path < b.package_path;
    });
    output = std::move(found);
    return true;
}
}
