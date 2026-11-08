#include "pkgmaster/core/job.hpp"
#include "pkgmaster/core/queue.hpp"
#include "pkgmaster/core/url_policy.hpp"
#include "pkgmaster/core/state_transition.hpp"
#include "pkgmaster/downloader/range_plan.hpp"
#include "pkgmaster/platform/capabilities.hpp"
#include <cassert>
int main() {
 pkgmaster::Job j; j.expected_bytes=200; j.received_bytes=50; assert(j.progress()>0.249&&j.progress()<0.251); assert(!j.terminal());
 pkgmaster::Queue q; auto id=q.add(j); assert(q.get(id).has_value()); assert(q.cancel(id)); assert(q.get(id)->terminal());
 pkgmaster::Job d; d.state=pkgmaster::JobState::Downloading; d.expected_bytes=100; auto did=q.add(d); assert(q.pause(did)); assert(q.resume(did)); assert(q.get(did)->state==pkgmaster::JobState::Resolving);
 pkgmaster::Job f; f.state=pkgmaster::JobState::Failed; f.received_bytes=12; f.error="network"; auto fid=q.add(f); assert(q.retry(fid)); assert(q.get(fid)->received_bytes==0&&q.get(fid)->error.empty()&&q.get(fid)->state==pkgmaster::JobState::Queued);
 std::string reason; assert(pkgmaster::is_allowed_download_url("https://example.com/file.pkg",reason)); assert(!pkgmaster::is_allowed_download_url("file:///etc/passwd",reason)); assert(!pkgmaster::is_allowed_download_url("http://127.0.0.1/file.pkg",reason));
 assert(pkgmaster::can_transition(pkgmaster::JobState::Queued,pkgmaster::JobState::Resolving)); assert(!pkgmaster::can_transition(pkgmaster::JobState::Completed,pkgmaster::JobState::Downloading));
 const auto plan=pkgmaster::make_range_plan(10*1024*1024,4,1024*1024); assert(plan.parts.size()==4&&plan.parts.front().first==0&&plan.parts.back().last==plan.total_bytes-1);
 for(std::size_t i=1;i<plan.parts.size();++i) assert(plan.parts[i-1].last+1==plan.parts[i].first);
 std::string e; assert(pkgmaster::validate_range_reply({0,99},{206,0,99,100,100,"etag"},100,e));
 assert(!pkgmaster::is_allowed_download_url("http://172.31.2.4/file.pkg",reason)); assert(!pkgmaster::is_allowed_download_url("https://192.168.1.8:8080/a.pkg",reason)); assert(!pkgmaster::is_allowed_download_url("http://[::1]/a.pkg",reason)); assert(!pkgmaster::is_allowed_download_url("http://[fd00::1]/a.pkg",reason));
 int a=0,b=0,c=0; assert(pkgmaster::parse_firmware_version("14.00",a,b,c)); assert(a==14&&b==0&&c==0); assert(pkgmaster::parse_firmware_version("11.02.03",a,b,c)); assert(a==11&&b==2&&c==3); assert(!pkgmaster::parse_firmware_version("14.x",a,b,c));
 pkgmaster::RuntimeInfo r; assert(!pkgmaster::evaluate_feature(r,pkgmaster::Capability::NetworkClient).available);
 r.available=static_cast<std::uint32_t>(pkgmaster::Capability::NetworkClient|pkgmaster::Capability::FileSystem); assert(pkgmaster::evaluate_feature(r,pkgmaster::Capability::NetworkClient).available); assert(!pkgmaster::evaluate_feature(r,pkgmaster::Capability::NetworkServer).available);
 return 0;
}