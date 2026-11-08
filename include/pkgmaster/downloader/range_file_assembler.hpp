#pragma once
#include "pkgmaster/downloader/range_plan.hpp"
#include <cstddef>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace pkgmaster {
// Writes independently downloaded ranges into one private staging file.
// Chunk writes are serialized and flushed before they are marked complete.
class RangeFileAssembler {
public:
    RangeFileAssembler() = default;
    ~RangeFileAssembler();
    RangeFileAssembler(const RangeFileAssembler&) = delete;
    RangeFileAssembler& operator=(const RangeFileAssembler&) = delete;

    bool create(const std::string& destination, const RangePlan& plan,
                std::uint64_t maximum_bytes, bool overwrite,
                std::string& error);
    bool write_chunk(std::size_t index, const void* data, std::size_t bytes,
                     std::string& error);
    bool chunk_complete(std::size_t index) const;
    bool complete() const;
    bool finalize(std::string& error);
    const std::string& temporary_path() const noexcept { return temporary_path_; }

private:
    mutable std::mutex mutex_;
    std::fstream file_;
    std::string destination_path_;
    std::string temporary_path_;
    std::vector<bool> completed_;
    std::uint64_t total_bytes_{};
    bool overwrite_{false};
    bool initialized_{false};
    bool published_{false};
};
}
