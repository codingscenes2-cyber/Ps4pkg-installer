#pragma once
#include "pkgmaster/downloader/range_plan.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace pkgmaster {
struct ResumeChunk {
    ByteRange range;
    bool complete{false};
    std::uint32_t checksum{};
    bool checksum_valid{false};
};
struct ResumeManifest {
    std::string source_url;
    std::string entity_tag;
    std::uint64_t total_bytes{};
    std::vector<ResumeChunk> chunks;
};
// Manifest is metadata only. The caller must retain the matching partial data file.
// ETag (or Last-Modified in a future version) must be revalidated before reusing bytes.
bool validate_resume_manifest(const ResumeManifest&, std::string& error);
// Marks one already-written range complete; caller must persist after data is flushed.
bool mark_resume_chunk_complete(ResumeManifest&, std::size_t chunk_index, std::string& error,\n                                std::uint32_t checksum = 0, bool checksum_valid = false);
std::vector<ByteRange> pending_resume_ranges(const ResumeManifest&);
bool resume_identity_matches(const ResumeManifest&, const std::string& source_url,
                            const std::string& strong_etag, std::uint64_t total_bytes);
bool save_resume_manifest(const std::string& path, const ResumeManifest&, std::string& error);
bool load_resume_manifest(const std::string& path, ResumeManifest&, std::string& error);
}
