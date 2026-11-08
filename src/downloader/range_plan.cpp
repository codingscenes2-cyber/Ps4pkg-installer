#include "pkgmaster/downloader/range_plan.hpp"
#include <algorithm>
namespace pkgmaster {
RangePlan make_range_plan(std::uint64_t total, unsigned workers, std::uint64_t minimum_part) {
 RangePlan p; p.total_bytes=total;
 if(!total || !workers || !minimum_part) return p;
 const auto useful=static_cast<unsigned>(std::min<std::uint64_t>(workers,(total+minimum_part-1)/minimum_part));
 const auto count=std::max(1u,useful); const auto base=total/count, rem=total%count;
 std::uint64_t cursor=0;
 for(unsigned i=0;i<count;++i){const auto len=base+(i<rem?1:0);p.parts.push_back({cursor,cursor+len-1});cursor+=len;}
 return p;
}
bool validate_range_reply(const ByteRange& req,const RangeReply& r,std::uint64_t total,std::string& reason){
 if(r.http_status!=206){reason="Origin did not return HTTP 206 Partial Content.";return false;}
 if(r.first!=req.first||r.last!=req.last){reason="Content-Range does not match requested interval.";return false;}
 if(r.total!=total){reason="Remote object size changed during download.";return false;}
 if(r.body_bytes!=req.length()){reason="Range body length is incomplete or oversized.";return false;}
 reason.clear();return true;
}
}