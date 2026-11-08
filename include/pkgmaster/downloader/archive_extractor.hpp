#pragma once
#include <atomic>
#include <cstdint>
#include <string>

namespace pkgmaster {
enum class ArchiveStatus { Completed, Cancelled, Failed, Unsupported };
struct ArchiveExtractOptions {
    std::string archive_path;
    std::string destination_directory;
    std::string password;
    std::uint64_t maximum_total_bytes{20ull*1024ull*1024ull*1024ull};
    std::uint64_t maximum_file_bytes{10ull*1024ull*1024ull*1024ull};
    std::uint64_t maximum_files{10000};
    std::uint32_t maximum_expansion_ratio{1000};
    const std::atomic_bool* cancel{nullptr};
};
struct ArchiveExtractResult {
    ArchiveStatus status{ArchiveStatus::Failed};
    std::uint64_t extracted_bytes{};
    std::uint64_t extracted_files{};
    std::uint64_t detected_pkg_files{};
    std::string error;
};
bool is_safe_archive_entry_path(const std::string& path, std::string& reason);
ArchiveExtractResult extract_archive_safely(const ArchiveExtractOptions& options);
}
