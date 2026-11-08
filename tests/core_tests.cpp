#include "pkgmaster/core/job.hpp"
#include "pkgmaster/core/queue.hpp"
#include "pkgmaster/core/url_policy.hpp"
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
    return 0;
}
