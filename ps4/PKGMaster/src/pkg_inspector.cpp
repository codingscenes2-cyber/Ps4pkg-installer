#include "pkg_inspector.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <vector>

namespace {
constexpr uint32_t kParamSfoEntryId = 0x00001000;
constexpr uint32_t kEncryptedEntryFlag = 0x80000000;
constexpr uint32_t kMaximumEntries = 4096;
constexpr uint32_t kMaximumSfoSize = 16u * 1024u * 1024u;
constexpr uint64_t kEntrySize = 0x20;

uint32_t read_be32(const uint8_t* bytes) {
    return (static_cast<uint32_t>(bytes[0]) << 24) |
           (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8) |
           static_cast<uint32_t>(bytes[3]);
}

bool read_at(std::ifstream& file, uint64_t offset, uint8_t* output, size_t size) {
    if (offset > static_cast<uint64_t>(std::numeric_limits<std::streamoff>::max())) {
        return false;
    }
    file.clear();
    file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!file) return false;
    file.read(reinterpret_cast<char*>(output), static_cast<std::streamsize>(size));
    return file.good() || (file.eof() && static_cast<size_t>(file.gcount()) == size);
}
}

bool inspect_pkg_param_sfo(const std::string& pkgPath,
                           pkgmaster::PackageMetadata& metadata,
                           std::string& error) {
    if (pkgPath.empty()) {
        error = "PKG path is empty.";
        return false;
    }

    std::ifstream file(pkgPath, std::ios::binary | std::ios::ate);
    if (!file) {
        error = "Could not open PKG file.";
        return false;
    }

    const std::streamoff end = file.tellg();
    if (end < 0 || static_cast<uint64_t>(end) < 0x20) {
        error = "PKG file is too small to contain a valid header.";
        return false;
    }
    const uint64_t fileSize = static_cast<uint64_t>(end);

    std::array<uint8_t, 0x20> header{};
    if (!read_at(file, 0, header.data(), header.size()) ||
        header[0] != 0x7f || header[1] != 'C' ||
        header[2] != 'N' || header[3] != 'T') {
        error = "Invalid PS4 PKG CNT header.";
        return false;
    }

    const uint32_t entryCount = read_be32(header.data() + 0x10);
    const uint32_t tableOffset = read_be32(header.data() + 0x18);
    if (entryCount == 0 || entryCount > kMaximumEntries ||
        tableOffset > fileSize ||
        static_cast<uint64_t>(entryCount) * kEntrySize > fileSize - tableOffset) {
        error = "PKG entry table offset or count is invalid.";
        return false;
    }

    std::array<uint8_t, kEntrySize> entry{};
    for (uint32_t i = 0; i < entryCount; ++i) {
        const uint64_t entryOffset = static_cast<uint64_t>(tableOffset) +
                                     static_cast<uint64_t>(i) * kEntrySize;
        if (!read_at(file, entryOffset, entry.data(), entry.size())) {
            error = "Could not read PKG entry table.";
            return false;
        }

        const uint32_t id = read_be32(entry.data());
        if (id != kParamSfoEntryId) continue;

        const uint32_t flags = read_be32(entry.data() + 8);
        const uint32_t dataOffset = read_be32(entry.data() + 16);
        const uint32_t dataSize = read_be32(entry.data() + 20);
        if ((flags & kEncryptedEntryFlag) != 0) {
            error = "PARAM.SFO entry is encrypted; this build only reads unencrypted metadata.";
            return false;
        }
        if (dataSize == 0 || dataSize > kMaximumSfoSize ||
            dataOffset > fileSize || dataSize > fileSize - dataOffset) {
            error = "PARAM.SFO entry has an invalid offset or size.";
            return false;
        }

        std::vector<uint8_t> bytes(dataSize);
        if (!read_at(file, dataOffset, bytes.data(), bytes.size())) {
            error = "Could not read PARAM.SFO entry data.";
            return false;
        }
        return pkgmaster::parse_param_sfo(bytes, metadata, error);
    }

    error = "PKG does not contain a PARAM.SFO entry.";
    return false;
}
