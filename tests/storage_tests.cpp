#include "pkgmaster/platform/storage.hpp"
#include <cassert>
#include <filesystem>
#include <string>
#include <vector>
int main() {
    namespace fs = std::filesystem;
    const auto base = fs::temp_directory_path() / "pkgmaster-storage-tests";
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base / "internal");
    fs::create_directories(base / "usb0");
    std::vector<pkgmaster::StorageCandidate> candidates{
        {"usb0", "USB drive", (base / "usb0").string(), pkgmaster::StorageKind::Usb},
        {"internal", "Internal", (base / "internal").string(), pkgmaster::StorageKind::Internal},
        {"missing", "Missing", (base / "missing").string(), pkgmaster::StorageKind::Usb}
    };
    std::vector<pkgmaster::StorageVolume> volumes;
    std::string error;
    assert(pkgmaster::discover_storage_volumes(candidates, volumes, error));
    assert(volumes.size() == 2);
    assert(volumes[0].id == "internal" && volumes[1].id == "usb0");
    assert(volumes[0].total_bytes >= volumes[0].available_bytes);
    assert(std::string(pkgmaster::storage_kind_name(volumes[1].kind)) == "usb");
    std::string child;
    assert(pkgmaster::make_storage_child_path((base / "internal").string(), "123-game.pkg", child, error));
    assert(child == (base / "internal" / "123-game.pkg").string());
    assert(!pkgmaster::make_storage_child_path((base / "internal").string(), "../escape.pkg", child, error));
    assert(!pkgmaster::make_storage_child_path((base / "internal").string(), "sub/escape.pkg", child, error));
    assert(!pkgmaster::make_storage_child_path((base / "internal").string(), "bad name.pkg", child, error));
    auto duplicate = candidates;
    duplicate[1].id = duplicate[0].id;
    auto unchanged = volumes;
    assert(!pkgmaster::discover_storage_volumes(duplicate, volumes, error));
    assert(volumes.size() == unchanged.size() && volumes[0].id == unchanged[0].id);
    auto too_many = std::vector<pkgmaster::StorageCandidate>(33, candidates[0]);
    for (std::size_t i = 0; i < too_many.size(); ++i) too_many[i].id = std::to_string(i);
    assert(!pkgmaster::discover_storage_volumes(too_many, volumes, error));
    fs::remove_all(base, ec);
}
