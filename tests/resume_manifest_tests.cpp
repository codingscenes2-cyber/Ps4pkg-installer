#include "pkgmaster/downloader/resume_manifest.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
#include <vector>
int main(){
 std::string error;
 pkgmaster::ResumeManifest m;
 m.source_url="https://cdn.example/game.pkg";m.entity_tag="\"abc-123\"";m.total_bytes=1000;
 m.chunks={{{0,249},true},{{250,499},true},{{500,749},false},{{750,999},false}};
 assert(pkgmaster::validate_resume_manifest(m,error));
 assert(pkgmaster::resume_identity_matches(m,m.source_url,m.entity_tag,m.total_bytes));
 assert(!pkgmaster::resume_identity_matches(m,m.source_url,"\"changed\"",m.total_bytes));
 auto pending=pkgmaster::pending_resume_ranges(m);assert(pending.size()==2&&pending[0].first==500);
 assert(pkgmaster::mark_resume_chunk_complete(m,2,error));
 pending=pkgmaster::pending_resume_ranges(m);assert(pending.size()==1&&pending[0].first==750);
 assert(!pkgmaster::mark_resume_chunk_complete(m,99,error));

 const std::string path="pkgmaster-resume-test.db";
 assert(pkgmaster::save_resume_manifest(path,m,error));
 pkgmaster::ResumeManifest restored;
 assert(pkgmaster::load_resume_manifest(path,restored,error));
 assert(restored.source_url==m.source_url&&restored.entity_tag==m.entity_tag);
 assert(restored.total_bytes==1000&&restored.chunks.size()==4);
 assert(restored.chunks[1].complete&&restored.chunks[2].complete&&!restored.chunks[3].complete);
 auto invalid=m;invalid.chunks[1].range.first=249;
 assert(!pkgmaster::validate_resume_manifest(invalid,error));
 invalid=m;invalid.chunks.back().range.last=1000;
 assert(!pkgmaster::validate_resume_manifest(invalid,error));
 invalid=m;invalid.chunks[0].range.last=std::numeric_limits<std::uint64_t>::max();
 assert(!pkgmaster::validate_resume_manifest(invalid,error));
 invalid=m;invalid.entity_tag="W/weak-tag";
 assert(!pkgmaster::validate_resume_manifest(invalid,error));
 invalid=m;invalid.source_url="http://127.0.0.1/private.pkg";
 assert(!pkgmaster::validate_resume_manifest(invalid,error));
 pkgmaster::ResumeManifest unchanged;unchanged.source_url="sentinel";
 assert(!pkgmaster::load_resume_manifest("missing-resume-file.db",unchanged,error));
 assert(unchanged.source_url=="sentinel");
 std::remove(path.c_str());return 0;
}
