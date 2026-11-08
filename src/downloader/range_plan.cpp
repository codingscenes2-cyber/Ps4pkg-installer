#include "pkgmaster/downloader/range_plan.hpp"
#include <algorithm>
#include <limits>
namespace pkgmaster {
namespace {
bool parse_decimal(const std::string& text,std::size_t& cursor,std::uint64_t& value){
 const auto start=cursor;std::uint64_t parsed=0;
 while(cursor<text.size()&&text[cursor]>='0'&&text[cursor]<='9'){
  const auto digit=static_cast<unsigned>(text[cursor]-'0');
  if(parsed>(std::numeric_limits<std::uint64_t>::max()-digit)/10)return false;
  parsed=parsed*10+digit;++cursor;
 }
 if(cursor==start)return false;value=parsed;return true;
}
}
RangePlan make_range_plan(std::uint64_t total, unsigned workers, std::uint64_t minimum_part) {
 RangePlan p; p.total_bytes=total;
 if(!total || !workers || !minimum_part) return p;
 const std::uint64_t useful_parts = total / minimum_part + (total % minimum_part ? 1u : 0u);
 const auto useful=static_cast<unsigned>(std::min<std::uint64_t>(workers,useful_parts));
 const auto count=std::max(1u,useful); const auto base=total/count, rem=total%count;
 std::uint64_t cursor=0;
 for(unsigned i=0;i<count;++i){const auto len=base+(i<rem?1:0);p.parts.push_back({cursor,cursor+len-1});cursor+=len;}
 return p;
}
bool parse_content_range_header(const std::string& header,std::uint64_t& first,
                                std::uint64_t& last,std::uint64_t& total,std::string& reason){
 if(header.rfind("bytes ",0)!=0){reason="Content-Range unit must be bytes.";return false;}
 std::size_t cursor=6;std::uint64_t parsed_first=0,parsed_last=0,parsed_total=0;
 if(!parse_decimal(header,cursor,parsed_first)||cursor>=header.size()||header[cursor++]!='-'||
    !parse_decimal(header,cursor,parsed_last)||cursor>=header.size()||header[cursor++]!='/'||
    !parse_decimal(header,cursor,parsed_total)||cursor!=header.size()||
    parsed_first>parsed_last||parsed_last>=parsed_total){
  reason="Malformed or out-of-bounds Content-Range header.";return false;
 }
 first=parsed_first;last=parsed_last;total=parsed_total;reason.clear();return true;
}
bool validate_range_reply(const ByteRange& req,const RangeReply& r,std::uint64_t total,std::string& reason){
 if(req.first>req.last || req.last>=total){reason="Requested byte interval is outside the object.";return false;}
 if(r.http_status!=206){reason="Origin did not return HTTP 206 Partial Content.";return false;}
 if(r.first!=req.first||r.last!=req.last){reason="Content-Range does not match requested interval.";return false;}
 if(r.total!=total){reason="Remote object size changed during download.";return false;}
 if(r.body_bytes!=req.length()){reason="Range body length is incomplete or oversized.";return false;}
 reason.clear();return true;
}
}
