#include "pkgmaster/core/queue.hpp"
#include <algorithm>

namespace pkgmaster {
std::uint64_t Queue::add(Job job) {
    std::lock_guard<std::mutex> lock(mutex_);
    job.id = next_id_++;
    jobs_.push_back(std::move(job));
    return jobs_.back().id;
}
bool Queue::update(const Job& job) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(jobs_.begin(), jobs_.end(),
                           [&](const Job& item) { return item.id == job.id; });
    if (it == jobs_.end()) return false;
    *it = job;
    return true;
}
bool Queue::pause(std::uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(jobs_.begin(), jobs_.end(),
                           [&](const Job& item) { return item.id == id; });
    if (it == jobs_.end() || it->terminal() ||
        it->state != JobState::Downloading) return false;
    it->state = JobState::Paused;
    return true;
}
bool Queue::cancel(std::uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(jobs_.begin(), jobs_.end(),
                           [&](const Job& item) { return item.id == id; });
    if (it == jobs_.end() || it->terminal()) return false;
    it->state = JobState::Cancelled;
    return true;
}
std::optional<Job> Queue::get(std::uint64_t id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(jobs_.begin(), jobs_.end(),
                           [&](const Job& item) { return item.id == id; });
    if (it == jobs_.end()) return std::nullopt;
    return *it;
}
std::vector<Job> Queue::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return jobs_;
}
}
