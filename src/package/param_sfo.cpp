#include "pkgmaster/package/param_sfo.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <limits>

namespace pkgmaster {
namespace {
constexpr std::size_t kHeaderSize=20, kIndexSize=16, kMaximumEntries=4096;
std::uint16_t u16(const std::vector<std::uint8_t>& b,std::size_t p) {
    return static_cast<std::uint16_t>(b[p]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(b[p+1])<<8);
}
std::uint32_t u32(const std::vector<std::uint8_t>& b,std::size_t p) {
    return static_cast<std::uint32_t>(b[p]) |
           (static_cast<std::uint32_t>(b[p+1])<<8) |
           (static_cast<std::uint32_t>(b[p+2])<<16) |
           (static_cast<std::uint32_t>(b[p+3])<<24);
}
bool add_fits(std::size_t a,std::size_t b,std::size_t limit) {
    return a<=limit&&b<=limit-a;
}
void assign_if_present(PackageMetadata& m,const std::string& key,const std::string& value) {
    if(key=="TITLE")m.title=value;
    else if(key=="TITLE_ID")m.title_id=value;
    else if(key=="APP_VER")m.app_version=value;
    else if(key=="CATEGORY")m.category=value;
    else if(key=="SYSTEM_VER")m.system_version=value;
}
}
bool parse_param_sfo(const std::vector<std::uint8_t>& b,PackageMetadata& output,std::string& error) {
    if(b.size()<kHeaderSize||b.size()>16u*1024u*1024u||
       b[0]!=0x00||b[1]!='P'||b[2]!='S'||b[3]!='F') {
        error="Invalid or oversized PARAM.SFO header.";return false;
    }
    const auto key_offset=u32(b,8),data_offset=u32(b,12),count=u32(b,16);
    if(count>kMaximumEntries||key_offset<kHeaderSize||data_offset<key_offset||
       !add_fits(key_offset,0,b.size())||!add_fits(data_offset,0,b.size())||
       count>(b.size()-kHeaderSize)/kIndexSize||
       !add_fits(kHeaderSize,static_cast<std::size_t>(count)*kIndexSize,b.size())) {
        error="PARAM.SFO table offsets or entry count are invalid.";return false;
    }
    const auto index_end=kHeaderSize+static_cast<std::size_t>(count)*kIndexSize;
    if(key_offset<index_end){error="PARAM.SFO key table overlaps its index table.";return false;}
    PackageMetadata parsed;
    for(std::size_t i=0;i<count;++i) {
        const auto p=kHeaderSize+i*kIndexSize;
        const auto key_rel=u16(b,p),format=u16(b,p+2);
        const auto length=u32(b,p+4),maximum=u32(b,p+8),data_rel=u32(b,p+12);
        if(length>maximum||maximum>16u*1024u*1024u||
           !add_fits(key_offset,key_rel,b.size())||
           !add_fits(key_offset+key_rel,1,b.size())) {
            error="PARAM.SFO entry contains invalid key or string lengths.";return false;
        }
        const auto key_start=static_cast<std::size_t>(key_offset)+key_rel;
        auto key_end=key_start;
        while(key_end<b.size()&&b[key_end]!=0&&key_end-key_start<=128)++key_end;
        if(key_end>=b.size()||key_end-key_start>128){error="PARAM.SFO key is unterminated or too long.";return false;}
        const std::string key(reinterpret_cast<const char*>(b.data()+key_start),key_end-key_start);
        if(format!=0x0204||length==0)continue;
        if(!add_fits(data_offset,data_rel,b.size())||
           !add_fits(static_cast<std::size_t>(data_offset)+data_rel,length,b.size())) {
            error="PARAM.SFO string data extends beyond the file.";return false;
        }
        const auto start=static_cast<std::size_t>(data_offset)+data_rel;
        std::size_t actual=length;
        while(actual>0&&b[start+actual-1]==0)--actual;
        std::string value(reinterpret_cast<const char*>(b.data()+start),actual);
        if(value.find('\0')!=std::string::npos){error="PARAM.SFO string contains an embedded NUL.";return false;}
        assign_if_present(parsed,key,value);
    }
    output=std::move(parsed);error.clear();return true;
}
bool read_param_sfo(const std::string& path,PackageMetadata& output,std::string& error) {
    if(path.empty()){error="PARAM.SFO path is empty.";return false;}
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in){error="Could not open PARAM.SFO.";return false;}
    const auto end=in.tellg();
    if(end<0||static_cast<std::uint64_t>(end)>16u*1024u*1024u){error="PARAM.SFO file size is invalid or exceeds 16 MiB.";return false;}
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    in.seekg(0);
    if(!bytes.empty()&&!in.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))){error="Could not read complete PARAM.SFO file.";return false;}
    return parse_param_sfo(bytes,output,error);
}
}
