#pragma once
#include "pkgmaster/downloader/range_plan.hpp"
#include <string>
#include <vector>
namespace pkgmaster {
struct ResumeChunk {
    ByteRange range;
    bool complete{false};
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
bool save_resume_manifest(const std::string& path, const ResumeManifest&, std::string& error);
bool load_resume_manifest(const std::string& path, ResumeManifest&, std::string& error);
}
