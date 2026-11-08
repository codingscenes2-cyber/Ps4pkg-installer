#pragma once
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace pkgmaster {
struct PackageRecord {
    std::uint64_t id{};
    std::string file_path;
    std::string file_name;
    std::uint64_t size_bytes{};
    std::string sha256;
    std::string source_url;
};
class PackageLibrary {
public:
    explicit PackageLibrary(std::string index_path) : index_path_(std::move(index_path)) {}
    bool load(std::string& error);
    bool save(std::string& error) const;
    bool add_package(const std::string& path, const std::string& source_url,
                     std::uint64_t& id, std::string& error);
    bool remove(std::uint64_t id, std::string& error);
    std::vector<PackageRecord> snapshot() const;
    std::vector<PackageRecord> search(const std::string& query) const;
    bool scan_directory(const std::string& root, std::size_t maximum_files,
                        std::size_t& added, std::string& error);
private:
    std::string index_path_;
    mutable std::mutex mutex_;
    std::vector<PackageRecord> records_;
    std::uint64_t next_id_{1};
};
}
