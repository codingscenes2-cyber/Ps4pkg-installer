#pragma once
#include "pkgmaster/core/job.hpp"
#include <mutex>
#include <optional>
#include <vector>
namespace pkgmaster {
class Queue {
public:
    std::uint64_t add(Job job);
    bool update(const Job& job);
    bool pause(std::uint64_t id);
    bool resume(std::uint64_t id);
    bool retry(std::uint64_t id);
    bool cancel(std::uint64_t id);
    std::optional<Job> get(std::uint64_t id) const;
    std::vector<Job> snapshot() const;
private:
    mutable std::mutex mutex_;
    std::vector<Job> jobs_;
    std::uint64_t next_id_{1};
};
}
