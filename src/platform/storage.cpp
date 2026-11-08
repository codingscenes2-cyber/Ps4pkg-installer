#include "pkgmaster/platform/storage.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace pkgmaster {
namespace fs = std::filesystem;
namespace {
bool simple_component(const std::string& name) {
    if (name.empty() || name.size() > 180 || name == "." || name == "..") return false;
    for (unsigned char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return false;
    }
    return true;
}
bool is_writable(const std::string& path) {
#if defined(_WIN32)
    return ::_access(path.c_str(), 2) == 0;
#else
    return ::access(path.c_str(), W_OK) == 0;
#endif
}
}
const char* storage_kind_name(StorageKind kind) noexcept {
    switch (kind) {
        case StorageKind::Internal: return "internal";
        case StorageKind::Usb: return "usb";
        case StorageKind::External: return "external";
    }
    return "unknown";
}
bool discover_storage_volumes(const std::vector<StorageCandidate>& candidates,
                              std::vector<StorageVolume>& output,
                              std::string& error) {
    error.clear();
    if (candidates.size() > 32) {
        error = "Storage candidate count exceeds the limit";
        return false;
    }
    std::set<std::string> ids;
    for (const auto& candidate : candidates) {
        if (candidate.id.empty() || candidate.id.size() > 64 ||
            candidate.label.size() > 128 || candidate.path.empty() ||
            !fs::path(candidate.path).is_absolute() ||
            !ids.insert(candidate.id).second) {
            error = "Storage candidate list contains invalid or duplicate entries";
            return false;
        }
    }
    std::vector<StorageVolume> found;
    for (const auto& candidate : candidates) {
        std::error_code ec;
        const auto status = fs::symlink_status(candidate.path, ec);
        if (ec || fs::is_symlink(status) || !fs::is_directory(status)) continue;
        const auto canonical = fs::canonical(candidate.path, ec);
        if (ec) continue;
        const auto capacity = fs::space(canonical, ec);
        if (ec) continue;
        StorageVolume volume;
        volume.id = candidate.id;
        volume.label = candidate.label;
        volume.path = canonical.string();
        volume.kind = candidate.kind;
        volume.total_bytes = capacity.capacity;
        volume.available_bytes = capacity.available;
        volume.writable = is_writable(volume.path);
        found.push_back(std::move(volume));
    }
    std::sort(found.begin(), found.end(), [](const StorageVolume& a, const StorageVolume& b) {
        return a.id < b.id;
    });
    output = std::move(found);
    return true;
}
bool make_storage_child_path(const std::string& root,
                             const std::string& filename,
                             std::string& output,
                             std::string& error) {
    error.clear();
    if (!simple_component(filename)) {
        error = "Filename must be a simple safe path component";
        return false;
    }
    std::error_code ec;
    const auto canonical_root = fs::canonical(root, ec);
    if (ec || !fs::is_directory(canonical_root, ec) || ec) {
        error = "Storage root is unavailable";
        return false;
    }
    const auto child = canonical_root / filename;
    const auto status = fs::symlink_status(child, ec);
    if (!ec && fs::is_symlink(status)) {
        error = "Refusing to write through a symbolic link";
        return false;
    }
    ec.clear();
    if (child.parent_path() != canonical_root) {
        error = "Resolved path escapes the configured storage root";
        return false;
    }
    output = child.string();
    return true;
}
}
