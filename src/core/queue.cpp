#include "pkgmaster/core/queue.hpp"
#include "pkgmaster/core/state_transition.hpp"
#include <algorithm>
#include <limits>
namespace pkgmaster {
std::uint64_t Queue::add(Job job) {
 std::lock_guard<std::mutex> lock(mutex_); job.id=next_id_++; jobs_.push_back(std::move(job)); return jobs_.back().id;
}
bool Queue::update(const Job& job) {
 std::lock_guard<std::mutex> lock(mutex_);
 auto it=std::find_if(jobs_.begin(),jobs_.end(),[&](const Job& x){return x.id==job.id;});
 if(it==jobs_.end()||!can_transition(it->state,job.state)) return false;
 if(job.received_bytes<it->received_bytes && job.state!=JobState::Queued) return false;
 *it=job; return true;
}
bool Queue::pause(std::uint64_t id) {
 std::lock_guard<std::mutex> lock(mutex_);
 auto it=std::find_if(jobs_.begin(),jobs_.end(),[&](const Job& x){return x.id==id;});
 if(it==jobs_.end()||!can_transition(it->state,JobState::Paused)) return false;
 it->state=JobState::Paused; return true;
}
bool Queue::resume(std::uint64_t id) {
 std::lock_guard<std::mutex> lock(mutex_);
 auto it=std::find_if(jobs_.begin(),jobs_.end(),[&](const Job& x){return x.id==id;});
 if(it==jobs_.end()||it->state!=JobState::Paused) return false;
 it->state=it->received_bytes?JobState::Downloading:JobState::Resolving; return true;
}
bool Queue::retry(std::uint64_t id) {
 std::lock_guard<std::mutex> lock(mutex_);
 auto it=std::find_if(jobs_.begin(),jobs_.end(),[&](const Job& x){return x.id==id;});
 if(it==jobs_.end()||it->state!=JobState::Failed) return false;
 it->received_bytes=0; it->error.clear(); it->state=JobState::Queued; return true;
}
bool Queue::cancel(std::uint64_t id) {
 std::lock_guard<std::mutex> lock(mutex_);
 auto it=std::find_if(jobs_.begin(),jobs_.end(),[&](const Job& x){return x.id==id;});
 if(it==jobs_.end()||it->terminal()) return false; it->state=JobState::Cancelled; return true;
}
bool Queue::restore(const std::vector<Job>& jobs) {
 std::lock_guard<std::mutex> lock(mutex_);
 std::uint64_t next = 1;
 for (std::size_t i = 0; i < jobs.size(); ++i) {
  const auto state = static_cast<unsigned>(jobs[i].state);
  if (!jobs[i].id || state > static_cast<unsigned>(JobState::Cancelled) ||
      (jobs[i].expected_bytes && jobs[i].received_bytes > jobs[i].expected_bytes) ||
      jobs[i].id == std::numeric_limits<std::uint64_t>::max()) return false;
  for (std::size_t j = 0; j < i; ++j)
   if (jobs[j].id == jobs[i].id) return false;
  if (jobs[i].id >= next) next = jobs[i].id + 1;
 }
 jobs_ = jobs;
 next_id_ = next;
 return true;
}
std::optional<Job> Queue::get(std::uint64_t id) const {
 std::lock_guard<std::mutex> lock(mutex_); auto it=std::find_if(jobs_.begin(),jobs_.end(),[&](const Job& x){return x.id==id;});
 if(it==jobs_.end()) return std::nullopt; return *it;
}
std::vector<Job> Queue::snapshot() const { std::lock_guard<std::mutex> lock(mutex_); return jobs_; }
}
