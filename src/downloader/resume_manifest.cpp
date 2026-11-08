#include "pkgmaster/downloader/resume_manifest.hpp"
#include "pkgmaster/core/url_policy.hpp"
#include <cstdio>
#include <fstream>
#include <limits>
#include <utility>

namespace pkgmaster {
namespace {
std::string hex_encode(const std::string& value) {
    static const char digits[] = "0123456789abcdef";
    std::string out; out.reserve(value.size()*2);
    for (unsigned char b : value) { out.push_back(digits[b>>4]); out.push_back(digits[b&15]); }
    return out;
}
int hex_digit(char c) {
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    if(c>='A'&&c<='F')return c-'A'+10;
    return -1;
}
bool hex_decode(const std::string& in,std::string& out) {
    if(in.size()%2||in.size()>2*1024*1024)return false;
    std::string value;value.reserve(in.size()/2);
    for(std::size_t i=0;i<in.size();i+=2){int a=hex_digit(in[i]),b=hex_digit(in[i+1]);if(a<0||b<0)return false;value.push_back(static_cast<char>((a<<4)|b));}
    out=std::move(value);return true;
}
bool parse_u64(const std::string& s,std::uint64_t& out) {
    if(s.empty())return false;std::uint64_t v=0;
    for(char c:s){if(c<'0'||c>'9')return false;unsigned d=static_cast<unsigned>(c-'0');if(v>(std::numeric_limits<std::uint64_t>::max()-d)/10)return false;v=v*10+d;}
    out=v;return true;
}
std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> fields;std::size_t start=0;
    for(;;){auto pos=line.find('\t',start);fields.push_back(line.substr(start,pos==std::string::npos?std::string::npos:pos-start));if(pos==std::string::npos)break;start=pos+1;}
    return fields;
}
bool strong_etag(const std::string& e) {
    if(e.size()<2||e.front()!='"'||e.back()!='"')return false;
    for(std::size_t i=1;i+1<e.size();++i){auto c=static_cast<unsigned char>(e[i]);if(c<=0x20||c=='"'||c==0x7f)return false;}
    return true;
}
}
bool validate_resume_manifest(const ResumeManifest& m,std::string& error) {
    if(m.source_url.size()>8192||m.entity_tag.size()>8192){error="Resume URL or ETag exceeds the metadata size limit.";return false;}
    std::string url_error;if(!is_allowed_download_url(m.source_url,url_error)){error="Resume source URL is not allowed: "+url_error;return false;}
    if(!strong_etag(m.entity_tag)){error="A valid quoted strong ETag is required before resuming partial bytes.";return false;}
    if(!m.total_bytes||m.chunks.empty()||m.chunks.size()>4096){error="Resume manifest has missing metadata or an invalid chunk count.";return false;}
    std::uint64_t cursor=0;
    for(const auto& c:m.chunks){if(c.range.first!=cursor||c.range.first>c.range.last||c.range.last>=m.total_bytes||c.range.last==std::numeric_limits<std::uint64_t>::max()){error="Resume chunks must form a contiguous, non-overlapping partition.";return false;}cursor=c.range.last+1;}
    if(cursor!=m.total_bytes){error="Resume chunks do not cover the complete remote object.";return false;}
    error.clear();return true;
}
bool mark_resume_chunk_complete(ResumeManifest& m,std::size_t i,std::string& error,std::uint32_t checksum,bool checksum_valid) {
    if(!validate_resume_manifest(m,error))return false;
    if(i>=m.chunks.size()){error="Resume chunk index is out of range.";return false;}
    m.chunks[i].complete=true;m.chunks[i].checksum=checksum;m.chunks[i].checksum_valid=checksum_valid;error.clear();return true;
}
std::vector<ByteRange> pending_resume_ranges(const ResumeManifest& m) {
    std::vector<ByteRange> out;std::string error;if(!validate_resume_manifest(m,error))return out;
    for(const auto& c:m.chunks)if(!c.complete)out.push_back(c.range);return out;
}
bool resume_identity_matches(const ResumeManifest& m,const std::string& url,const std::string& etag,std::uint64_t bytes) {
    std::string error;return validate_resume_manifest(m,error)&&strong_etag(etag)&&m.source_url==url&&m.entity_tag==etag&&m.total_bytes==bytes;
}
bool save_resume_manifest(const std::string& path,const ResumeManifest& m,std::string& error) {
    if(path.empty()){error="Resume manifest path is empty.";return false;}
    if(!validate_resume_manifest(m,error))return false;
    const std::string temp=path+".tmp";std::ofstream out(temp,std::ios::binary|std::ios::trunc);
    if(!out){error="Could not create resume manifest temporary file.";return false;}
    out<<"PKGMASTER_RESUME\t2\n"<<m.total_bytes<<'\t'<<hex_encode(m.source_url)<<'\t'<<hex_encode(m.entity_tag)<<'\n';
    for(const auto& c:m.chunks)out<<c.range.first<<'\t'<<c.range.last<<'\t'<<(c.complete?1:0)<<'\t'<<(c.checksum_valid?std::to_string(c.checksum):"-")<<'\n';
    out.flush();bool ok=static_cast<bool>(out);out.close();ok=ok&&!out.fail();
    if(!ok){error="Could not write resume manifest.";std::remove(temp.c_str());return false;}
    if(std::rename(temp.c_str(),path.c_str())!=0){error="Could not replace resume manifest.";std::remove(temp.c_str());return false;}
    error.clear();return true;
}
bool load_resume_manifest(const std::string& path,ResumeManifest& output,std::string& error) {
    std::ifstream in(path,std::ios::binary);if(!in){error="Could not open resume manifest.";return false;}
    std::string line;if(!std::getline(in,line)){error="Unsupported or corrupt resume manifest header.";return false;}
    const bool v2=line=="PKGMASTER_RESUME\t2";
    if(!v2&&line!="PKGMASTER_RESUME\t1"){error="Unsupported or corrupt resume manifest header.";return false;}
    ResumeManifest parsed;
    if(!std::getline(in,line)||line.size()>2*1024*1024){error="Invalid resume manifest metadata.";return false;}
    auto meta=split_tabs(line);
    if(meta.size()!=3||!parse_u64(meta[0],parsed.total_bytes)||!hex_decode(meta[1],parsed.source_url)||!hex_decode(meta[2],parsed.entity_tag)){error="Invalid resume manifest metadata.";return false;}
    while(std::getline(in,line)){
        if(line.empty())continue;if(line.size()>256||parsed.chunks.size()>=4096){error="Resume manifest exceeds safety limits.";return false;}
        auto fields=split_tabs(line);std::uint64_t first=0,last=0,done=0,checksum=0;
        if(fields.size()!=(v2?4u:3u)||!parse_u64(fields[0],first)||!parse_u64(fields[1],last)||!parse_u64(fields[2],done)||done>1||
           (v2&&fields[3]!="-"&&(!parse_u64(fields[3],checksum)||checksum>0xffffffffu))){error="Invalid resume chunk record.";return false;}
        ResumeChunk chunk;chunk.range={first,last};chunk.complete=done==1;
        if(v2&&fields[3]!="-"){chunk.checksum=static_cast<std::uint32_t>(checksum);chunk.checksum_valid=true;}
        parsed.chunks.push_back(chunk);
    }
    if(in.bad()){error="Failed while reading resume manifest.";return false;}
    if(!validate_resume_manifest(parsed,error))return false;
    output=std::move(parsed);error.clear();return true;
}
}
