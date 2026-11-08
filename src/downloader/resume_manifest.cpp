#include "pkgmaster/downloader/resume_manifest.hpp"
#include "pkgmaster/core/url_policy.hpp"

#include <cstdio>
#include <fstream>
#include <limits>
#include <utility>

namespace pkgmaster {
namespace {

std::string hex_encode(const std::string& value) {
    static const char digits[] = "0123456789abcdef";
    std::string encoded;
    encoded.reserve(value.size() * 2);
    for (unsigned char byte : value) {
        encoded.push_back(digits[byte >> 4]);
        encoded.push_back(digits[byte & 0x0f]);
    }
    return encoded;
}

int hex_digit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool hex_decode(const std::string& encoded, std::string& output) {
    constexpr std::size_t kMaximumEncodedBytes = 2 * 1024 * 1024;
    if (encoded.size() % 2 != 0 || encoded.size() > kMaximumEncodedBytes)
        return false;

    std::string decoded;
    decoded.reserve(encoded.size() / 2);
    for (std::size_t i = 0; i < encoded.size(); i += 2) {
        const int high = hex_digit(encoded[i]);
        const int low = hex_digit(encoded[i + 1]);
        if (high < 0 || low < 0) return false;
        decoded.push_back(static_cast<char>((high << 4) | low));
    }
    output = std::move(decoded);
    return true;
}

bool parse_u64(const std::string& text, std::uint64_t& output) {
    if (text.empty()) return false;

    std::uint64_t value = 0;
    for (char character : text) {
        if (character < '0' || character > '9') return false;
        const auto digit = static_cast<unsigned>(character - '0');
        if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
            return false;
        value = value * 10 + digit;
    }
    output = value;
    return true;
}

std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    for (;;) {
        const auto tab = line.find('\t', start);
        fields.push_back(line.substr(start, tab == std::string::npos
                                                ? std::string::npos
                                                : tab - start));
        if (tab == std::string::npos) break;
        start = tab + 1;
    }
    return fields;
}

bool is_strong_etag(const std::string& etag) {
    if (etag.size() < 2 || etag.front() != '"' || etag.back() != '"')
        return false;
    for (std::size_t i = 1; i + 1 < etag.size(); ++i) {
        const auto byte = static_cast<unsigned char>(etag[i]);
        if (byte <= 0x20 || byte == '"' || byte == 0x7f) return false;
    }
    return true;
}

}  // namespace

bool validate_resume_manifest(const ResumeManifest& manifest,
                              std::string& error) {
    std::string url_error;
    if (!is_allowed_download_url(manifest.source_url, url_error)) {
        error = "Resume source URL is not allowed: " + url_error;
        return false;
    }
    if (!is_strong_etag(manifest.entity_tag)) {
        error = "A valid quoted strong ETag is required before resuming partial bytes.";
        return false;
    }
    if (manifest.total_bytes == 0 || manifest.chunks.empty() ||
        manifest.chunks.size() > 4096) {
        error = "Resume manifest has missing metadata or an invalid chunk count.";
        return false;
    }

    std::uint64_t cursor = 0;
    for (const auto& chunk : manifest.chunks) {
        if (chunk.range.first != cursor ||
            chunk.range.first > chunk.range.last ||
            chunk.range.last >= manifest.total_bytes) {
            error = "Resume chunks must form a contiguous, non-overlapping partition.";
            return false;
        }
        if (chunk.range.last == std::numeric_limits<std::uint64_t>::max()) {
            error = "Resume chunk end overflows.";
            return false;
        }
        cursor = chunk.range.last + 1;
    }

    if (cursor != manifest.total_bytes) {
        error = "Resume chunks do not cover the complete remote object.";
        return false;
    }
    error.clear();
    return true;
}

bool mark_resume_chunk_complete(ResumeManifest& manifest,
                                std::size_t chunk_index,
                                std::string& error) {
    if (!validate_resume_manifest(manifest, error)) return false;
    if (chunk_index >= manifest.chunks.size()) {
        error = "Resume chunk index is out of range.";
        return false;
    }
    manifest.chunks[chunk_index].complete = true;
    error.clear();
    return true;
}

std::vector<ByteRange> pending_resume_ranges(const ResumeManifest& manifest) {
    std::vector<ByteRange> ranges;
    std::string error;
    if (!validate_resume_manifest(manifest, error)) return ranges;
    for (const auto& chunk : manifest.chunks) {
        if (!chunk.complete) ranges.push_back(chunk.range);
    }
    return ranges;
}

bool resume_identity_matches(const ResumeManifest& manifest,
                             const std::string& source_url,
                             const std::string& strong_etag,
                             std::uint64_t total_bytes) {
    std::string error;
    return validate_resume_manifest(manifest, error) &&
           is_strong_etag(strong_etag) &&
           manifest.source_url == source_url &&
           manifest.entity_tag == strong_etag &&
           manifest.total_bytes == total_bytes;
}

bool save_resume_manifest(const std::string& path,
                          const ResumeManifest& manifest,
                          std::string& error) {
    if (path.empty()) {
        error = "Resume manifest path is empty.";
        return false;
    }
    if (!validate_resume_manifest(manifest, error)) return false;

    const std::string temporary = path + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "Could not create resume manifest temporary file.";
        return false;
    }

    output << "PKGMASTER_RESUME\t1\n"
           << manifest.total_bytes << '\t'
           << hex_encode(manifest.source_url) << '\t'
           << hex_encode(manifest.entity_tag) << '\n';
    for (const auto& chunk : manifest.chunks) {
        output << chunk.range.first << '\t' << chunk.range.last << '\t'
               << (chunk.complete ? 1 : 0) << '\n';
    }

    output.flush();
    bool written = static_cast<bool>(output);
    output.close();
    written = written && !output.fail();
    if (!written) {
        error = "Could not write resume manifest.";
        std::remove(temporary.c_str());
        return false;
    }
    if (std::rename(temporary.c_str(), path.c_str()) != 0) {
        error = "Could not replace resume manifest.";
        std::remove(temporary.c_str());
        return false;
    }
    error.clear();
    return true;
}

bool load_resume_manifest(const std::string& path,
                          ResumeManifest& output,
                          std::string& error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "Could not open resume manifest.";
        return false;
    }

    std::string line;
    if (!std::getline(input, line) || line != "PKGMASTER_RESUME\t1") {
        error = "Unsupported or corrupt resume manifest header.";
        return false;
    }

    ResumeManifest parsed;
    if (!std::getline(input, line) || line.size() > 2 * 1024 * 1024) {
        error = "Invalid resume manifest metadata.";
        return false;
    }
    const auto metadata = split_tabs(line);
    if (metadata.size() != 3 ||
        !parse_u64(metadata[0], parsed.total_bytes) ||
        !hex_decode(metadata[1], parsed.source_url) ||
        !hex_decode(metadata[2], parsed.entity_tag)) {
        error = "Invalid resume manifest metadata.";
        return false;
    }

    while (std::getline(input, line)) {
        if (line.empty()) continue;
        if (line.size() > 256 || parsed.chunks.size() >= 4096) {
            error = "Resume manifest exceeds safety limits.";
            return false;
        }

        const auto fields = split_tabs(line);
        std::uint64_t first = 0;
        std::uint64_t last = 0;
        std::uint64_t complete = 0;
        if (fields.size() != 3 ||
            !parse_u64(fields[0], first) ||
            !parse_u64(fields[1], last) ||
            !parse_u64(fields[2], complete) ||
            complete > 1) {
            error = "Invalid resume chunk record.";
            return false;
        }
        parsed.chunks.push_back({{first, last}, complete == 1});
    }

    if (input.bad()) {
        error = "Failed while reading resume manifest.";
        return false;
    }
    if (!validate_resume_manifest(parsed, error)) return false;

    output = std::move(parsed);
    error.clear();
    return true;
}

}  // namespace pkgmaster
