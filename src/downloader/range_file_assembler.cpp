#include "pkgmaster/downloader/range_file_assembler.hpp"

#include <cstdio>
#include <fstream>
#include <limits>

namespace pkgmaster {
namespace {

bool valid_plan(const RangePlan& plan) {
    if (plan.total_bytes == 0 || plan.parts.empty() || plan.parts.size() > 4096)
        return false;
    std::uint64_t cursor = 0;
    for (const auto& part : plan.parts) {
        if (part.first != cursor || part.first > part.last ||
            part.last >= plan.total_bytes ||
            part.last == std::numeric_limits<std::uint64_t>::max())
            return false;
        cursor = part.last + 1;
    }
    return cursor == plan.total_bytes;
}

bool path_exists(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return file.good();
}

}  // namespace

RangeFileAssembler::~RangeFileAssembler() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) file_.close();
}

bool RangeFileAssembler::create(const std::string& destination,
                                const RangePlan& plan,
                                std::uint64_t maximum_bytes,
                                bool overwrite,
                                std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        error = "Range assembler has already been initialized.";
        return false;
    }
    if (destination.empty() || !valid_plan(plan) ||
        plan.total_bytes > maximum_bytes ||
        plan.total_bytes > static_cast<std::uint64_t>(
            std::numeric_limits<std::streamoff>::max())) {
        error = "Invalid destination, range plan or maximum file size.";
        return false;
    }
    if (!overwrite && path_exists(destination)) {
        error = "Destination already exists; overwrite was not enabled.";
        return false;
    }

    destination_path_ = destination;
    temporary_path_ = destination + ".part";
    if (path_exists(temporary_path_)) {
        error = "A staging file already exists; refusing to destroy possible recovery data.";
        return false;
    }

    file_.open(temporary_path_, std::ios::binary | std::ios::in |
                                  std::ios::out | std::ios::trunc);
    if (!file_) {
        error = "Could not create range staging file.";
        return false;
    }

    file_.seekp(static_cast<std::streamoff>(plan.total_bytes - 1));
    file_.put('\0');
    file_.flush();
    if (!file_) {
        file_.close();
        std::remove(temporary_path_.c_str());
        error = "Could not allocate the range staging file.";
        return false;
    }

    overwrite_ = overwrite;
    completed_.assign(plan.parts.size(), false);
    plan_ = plan;
    initialized_ = true;
    error.clear();
    return true;
}

bool RangeFileAssembler::write_chunk(std::size_t index, const void* data,
                                     std::size_t bytes, std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || published_ || index >= plan_.parts.size()) {
        error = "Range assembler is not ready or chunk index is invalid.";
        return false;
    }
    const auto& range = plan_.parts[index];
    const auto expected = range.length();
    if (!data || expected != bytes ||
        expected > static_cast<std::uint64_t>(
            std::numeric_limits<std::streamsize>::max())) {
        error = "Chunk data length does not match its assigned byte range.";
        return false;
    }
    if (completed_[index]) {
        error = "This range has already been written.";
        return false;
    }

    file_.clear();
    file_.seekp(static_cast<std::streamoff>(range.first));
    file_.write(static_cast<const char*>(data),
                static_cast<std::streamsize>(bytes));
    file_.flush();
    if (!file_) {
        error = "Could not write or flush range data.";
        return false;
    }
    completed_[index] = true;
    error.clear();
    return true;
}

bool RangeFileAssembler::chunk_complete(std::size_t index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_ && index < completed_.size() && completed_[index];
}

bool RangeFileAssembler::complete() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || completed_.empty()) return false;
    for (bool done : completed_) if (!done) return false;
    return true;
}

bool RangeFileAssembler::finalize(std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || published_) {
        error = "Range assembler is not ready to publish.";
        return false;
    }
    for (bool done : completed_) {
        if (!done) {
            error = "Cannot publish while ranges are incomplete.";
            return false;
        }
    }

    file_.flush();
    if (!file_) {
        error = "Could not flush completed package data.";
        return false;
    }
    file_.close();
    if (file_.fail()) {
        error = "Could not close completed package data.";
        return false;
    }
    if (!overwrite_ && path_exists(destination_path_)) {
        error = "Destination appeared while downloading; refusing to overwrite it.";
        return false;
    }
    if (std::rename(temporary_path_.c_str(), destination_path_.c_str()) != 0) {
        error = "Could not atomically publish the completed package file.";
        return false;
    }
    published_ = true;
    error.clear();
    return true;
}

}  // namespace pkgmaster
