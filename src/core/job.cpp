#include "pkgmaster/core/job.hpp"
#include <algorithm>

namespace pkgmaster {
bool Job::terminal() const noexcept {
    return state == JobState::Completed || state == JobState::Failed ||
           state == JobState::Cancelled;
}
double Job::progress() const noexcept {
    if (state == JobState::Completed) return 1.0;
    if (expected_bytes == 0) return 0.0;
    const auto ratio = static_cast<double>(received_bytes) /
                       static_cast<double>(expected_bytes);
    return std::max(0.0, std::min(1.0, ratio));
}
}
