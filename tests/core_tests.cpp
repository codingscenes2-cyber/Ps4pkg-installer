#include "pkgmaster/core/job.hpp"
#include "pkgmaster/core/queue.hpp"
#include "pkgmaster/core/url_policy.hpp"
#include "pkgmaster/core/state_transition.hpp"
#include "pkgmaster/downloader/range_plan.hpp"
#include <cassert>

int main() {
    pkgmaster::Job j;
    j.expected_bytes = 200;
    j.received_bytes = 50;
    assert(j.progress() > 0.249 && j.progress() < 0.251);
    assert(!j.terminal());

    pkgmaster::Queue q;
    const auto id = q.add(j);
    assert(q.get(id).has_value());
    assert(q.cancel(id));
    assert(q.get(id)->terminal());

    std::string reason;
    assert(pkgmaster::is_allowed_download_url("https://example.com/file.pkg", reason));
    assert(!pkgmaster::is_allowed_download_url("file:///etc/passwd", reason));
    assert(!pkgmaster::is_allowed_download_url("http://127.0.0.1/file.pkg", reason));
    assert(pkgmaster::can_transition(pkgmaster::JobState::Queued, pkgmaster::JobState::Resolving));
    assert(!pkgmaster::can_transition(pkgmaster::JobState::Completed, pkgmaster::JobState::Downloading));
    const auto plan = pkgmaster::make_range_plan(10 * 1024 * 1024, 4, 1024 * 1024);
    assert(plan.parts.size() == 4 && plan.parts.front().first == 0 && plan.parts.back().last == plan.total_bytes - 1);
    for (std::size_t i=1; i<plan.parts.size(); ++i) assert(plan.parts[i-1].last+1 == plan.parts[i].first);
    std::string range_error;
    assert(pkgmaster::validate_range_reply({0,99},{206,0,99,100,100,"etag"},100,range_error));
    return 0;
}
