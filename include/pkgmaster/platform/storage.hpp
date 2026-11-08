#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace pkgmaster {
enum class StorageKind { Internal, Usb, External };
struct StorageCandidate {
    std::string id;
    std::string label;
    std::string path;
    StorageKind kind{StorageKind::External};
};
struct StorageVolume {
    std::string id;
    std::string label;
    std::string path;
    StorageKind kind{StorageKind::External};
    std::uint64_t total_bytes{};
    std::uint64_t available_bytes{};
    bool writable{false};
};
// Probes only caller-configured roots. Missing, unmounted or non-directory
// candidates are skipped; malformed candidate lists fail transactionally.
bool discover_storage_volumes(const std::vector<StorageCandidate>& candidates,
                              std::vector<StorageVolume>& output,
                              std::string& error);
// Safely joins a simple filename beneath an existing configured storage root.
// Does not create files or accept client-supplied path fragments.
bool make_storage_child_path(const std::string& root,
                             const std::string& filename,
                             std::string& output,
                             std::string& error);
const char* storage_kind_name(StorageKind kind) noexcept;
}
