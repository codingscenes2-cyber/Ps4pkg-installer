#include "pkgmaster/package/library_index_store.hpp"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace pkgmaster {
namespace {
constexpr std::size_t kMaxRecords = 100000;
constexpr std::size_t kMaxStringBytes = 1024 * 1024;
constexpr std::uint64_t kMaxIndexBytes = 64ull * 1024ull * 1024ull;
std::string encode(const std::string& value) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(value.size() * 2);
    for (unsigned char c : value) {
        out.push_back(digits[c >> 4]);
        out.push_back(digits[c & 15]);
    }
    return out;
}
int nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
bool decode(const std::string& value, std::string& output) {
    if ((value.size() & 1u) || value.size() / 2 > kMaxStringBytes) return false;
    std::string parsed;
    parsed.reserve(value.size() / 2);
    for (std::size_t i = 0; i < value.size(); i += 2) {
        const int hi = nibble(value[i]), lo = nibble(value[i + 1]);
        if (hi < 0 || lo < 0) return false;
        parsed.push_back(static_cast<char>((hi << 4) | lo));
    }
    output = std::move(parsed);
    return true;
}
bool parse_u64(const std::string& value, std::uint64_t& output) {
    if (value.empty()) return false;
    std::uint64_t parsed = 0;
    for (char c : value) {
        if (c < '0' || c > '9') return false;
        const auto digit = static_cast<unsigned>(c - '0');
        if (parsed > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) return false;
        parsed = parsed * 10 + digit;
    }
    output = parsed;
    return true;
}
bool split(const std::string& line, std::vector<std::string>& fields) {
    fields.clear();
    std::size_t start = 0;
    for (;;) {
        const auto pos = line.find('\t', start);
        fields.push_back(line.substr(start, pos == std::string::npos ? pos : pos - start));
        if (pos == std::string::npos) return fields.size() == 8;
        start = pos + 1;
        if (fields.size() > 8) return false;
    }
}
bool valid_package(const LibraryPackage& item) {
    const std::string* values[] = {&item.metadata.title, &item.metadata.title_id,
        &item.metadata.app_version, &item.metadata.category,
        &item.metadata.system_version, &item.package_path, &item.param_sfo_path};
    for (const auto* value : values) if (value->size() > kMaxStringBytes) return false;
    return !item.package_path.empty() && !item.param_sfo_path.empty();
}
}
bool PackageLibraryIndexStore::save(const std::string& path,
                                    const std::vector<LibraryPackage>& packages,
                                    std::string& error) {
    error.clear();
    if (path.empty() || packages.size() > kMaxRecords) {
        error = "Invalid package index path or record count";
        return false;
    }
    std::vector<LibraryPackage> sorted = packages;
    std::sort(sorted.begin(), sorted.end(), [](const LibraryPackage& a, const LibraryPackage& b) {
        if (a.metadata.title_id != b.metadata.title_id)
            return a.metadata.title_id < b.metadata.title_id;
        return a.package_path < b.package_path;
    });
    std::set<std::string> paths;
    for (const auto& item : sorted) {
        if (!valid_package(item) || !paths.insert(item.param_sfo_path).second) {
            error = "Package index contains invalid or duplicate paths";
            return false;
        }
    }
    const std::string temporary = path + ".tmp";
    std::uint64_t projected_bytes = std::string("PKGMASTER_LIBRARY\\t1\\n").size();
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) { error = "Cannot open temporary package index"; return false; }
        out << "PKGMASTER_LIBRARY\t1\n";
        for (const auto& item : sorted) {
            const std::string* values[] = {&item.metadata.title, &item.metadata.title_id,
                &item.metadata.app_version, &item.metadata.category,
                &item.metadata.system_version, &item.package_path, &item.param_sfo_path};
            std::uint64_t record_bytes = std::to_string(item.param_sfo_bytes).size() + 9;
            for (const auto* value : values) record_bytes += static_cast<std::uint64_t>(value->size()) * 2;
            if (record_bytes > kMaxIndexBytes - projected_bytes) {
                error = "Package index exceeds the size limit";
                out.close();
                std::remove(temporary.c_str());
                return false;
            }
            projected_bytes += record_bytes;
            out << item.param_sfo_bytes << '\t'
                << encode(item.metadata.title) << '\t'
                << encode(item.metadata.title_id) << '\t'
                << encode(item.metadata.app_version) << '\t'
                << encode(item.metadata.category) << '\t'
                << encode(item.metadata.system_version) << '\t'
                << encode(item.package_path) << '\t'
                << encode(item.param_sfo_path) << '\n';
        }
        out.flush();
        if (!out) {
            error = "Failed while writing package index";
            out.close();
            std::remove(temporary.c_str());
            return false;
        }
        const auto position = out.tellp();
        if (position < 0 || static_cast<std::uint64_t>(position) > kMaxIndexBytes) {
            error = "Package index exceeds the size limit";
            out.close();
            std::remove(temporary.c_str());
            return false;
        }
    }
    if (std::rename(temporary.c_str(), path.c_str()) != 0) {
        error = "Could not atomically replace package index";
        std::remove(temporary.c_str());
        return false;
    }
    return true;
}
bool PackageLibraryIndexStore::load(const std::string& path,
                                    std::vector<LibraryPackage>& packages,
                                    std::string& error) {
    error.clear();
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) { error = "Cannot open package index"; return false; }
    const auto end = in.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) > kMaxIndexBytes) {
        error = "Package index size is invalid or exceeds the limit";
        return false;
    }
    in.seekg(0);
    std::string header;
    if (!std::getline(in, header) || header != "PKGMASTER_LIBRARY\t1") {
        error = "Unsupported or corrupt package index header";
        return false;
    }
    std::vector<LibraryPackage> parsed;
    std::set<std::string> paths;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (parsed.size() >= kMaxRecords || line.size() > 8 * kMaxStringBytes + 128) {
            error = "Package index record limit exceeded";
            return false;
        }
        std::vector<std::string> fields;
        LibraryPackage item;
        if (!split(line, fields) ||
            !parse_u64(fields[0], item.param_sfo_bytes) ||
            !decode(fields[1], item.metadata.title) ||
            !decode(fields[2], item.metadata.title_id) ||
            !decode(fields[3], item.metadata.app_version) ||
            !decode(fields[4], item.metadata.category) ||
            !decode(fields[5], item.metadata.system_version) ||
            !decode(fields[6], item.package_path) ||
            !decode(fields[7], item.param_sfo_path) ||
            !valid_package(item) || !paths.insert(item.param_sfo_path).second) {
            error = "Invalid or duplicate package index record";
            return false;
        }
        parsed.push_back(std::move(item));
    }
    if (in.bad()) { error = "Failed while reading package index"; return false; }
    std::sort(parsed.begin(), parsed.end(), [](const LibraryPackage& a, const LibraryPackage& b) {
        if (a.metadata.title_id != b.metadata.title_id)
            return a.metadata.title_id < b.metadata.title_id;
        return a.package_path < b.package_path;
    });
    packages = std::move(parsed);
    return true;
}
}
