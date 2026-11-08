#include "pkgmaster/downloader/resume_manifest.hpp"
#include "pkgmaster/core/url_policy.hpp"
#include <cstdio>
#include <fstream>
#include <limits>
#include <utility>
namespace pkgmaster {
namespace {
std::string encode(const std::string& s) {
 static const char h[]="0123456789abcdef"; std::string o; o.reserve(s.size()*2);
 for(unsigned char c:s){o.push_back(h[c>>4]);o.push_back(h[c&15]);} return o;
}
int digit(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
bool decode(const std::string& s,std::string& out){
 if(s.size()%2 || s.size()>2*1024*1024)return false; std::string v;v.reserve(s.size()/2);
 for(std::size_t i=0;i<s.size();i+=2){int a=digit(s[i]),b=digit(s[i+1]);if(a<0||b<0)return false;v.push_back(static_cast<char>((a<<4)|b));}
 out=std::move(v);return true;
}
bool number(const std::string& s,std::uint64_t& v){
 if(s.empty())return false;std::uint64_t n=0;
 for(char c:s){if(c<'0'||c>'9')return false;unsigned d=static_cast<unsigned>(c-'0');if(n>(std::numeric_limits<std::uint64_t>::max()-d)/10)return false;n=n*10+d;}v=n;return true;
}
bool split(const std::string& s,std::vector<std::string>& f){
 f.clear();std::size_t p=0;for(;;){auto q=s.find('\t',p);f.push_back(s.substr(p,q==std::string::npos?q:q-p));if(q==std::string::npos)break;p=q+1;}return true;
}
}
bool validate_resume_manifest(const ResumeManifest& m,std::string& error){
 std::string url_error;
 if(!is_allowed_download_url(m.source_url,url_error)){error="Resume source URL is not allowed: "+url_error;return false;}
 if(m.entity_tag.empty()||m.entity_tag.rfind("W/",0)==0){error="A strong ETag is required before resuming partial bytes.";return false;}
 if(m.total_bytes==0||m.chunks.empty()||m.chunks.size()>4096){error="Resume manifest has missing metadata or an invalid chunk count.";return false;}
 std::uint64_t cursor=0;
 for(const auto& c:m.chunks){
  if(c.range.first!=cursor||c.range.first>c.range.last||c.range.last>=m.total_bytes){error="Resume chunks must form a contiguous, non-overlapping partition.";return false;}
  if(c.range.last==std::numeric_limits<std::uint64_t>::max()){error="Resume chunk end overflows.";return false;}
  cursor=c.range.last+1;
 }
 if(cursor!=m.total_bytes){error="Resume chunks do not cover the complete remote object.";return false;}
 error.clear();return true;
}
bool save_resume_manifest(const std::string& path,const ResumeManifest& m,std::string& error){
 if(path.empty()){error="Resume manifest path is empty.";return false;}
 if(!validate_resume_manifest(m,error))return false;
 const std::string tmp=path+".tmp";std::ofstream out(tmp,std::ios::binary|std::ios::trunc);
 if(!out){error="Could not create resume manifest temporary file.";return false;}
 out<<"PKGMASTER_RESUME\t1\n"<<m.total_bytes<<'\t'<<encode(m.source_url)<<'\t'<<encode(m.entity_tag)<<'\n';
 for(const auto& c:m.chunks)out<<c.range.first<<'\t'<<c.range.last<<'\t'<<(c.complete?1:0)<<'\n';
 out.flush();bool ok=static_cast<bool>(out);out.close();ok=ok&&!out.fail();
 if(!ok){error="Could not write resume manifest.";std::remove(tmp.c_str());return false;}
 if(std::rename(tmp.c_str(),path.c_str())!=0){error="Could not replace resume manifest.";std::remove(tmp.c_str());return false;}
 error.clear();return true;
}
bool load_resume_manifest(const std::string& path,ResumeManifest& output,std::string& error){
 std::ifstream in(path,std::ios::binary);if(!in){error="Could not open resume manifest.";return false;}
 std::string line;if(!std::getline(in,line)||line!="PKGMASTER_RESUME\t1"){error="Unsupported or corrupt resume manifest header.";return false;}
 ResumeManifest parsed;std::vector<std::string> f;
 if(!std::getline(in,line)||line.size()>2*1024*1024||!split(line,f)||f.size()!=3||
    !number(f[0],parsed.total_bytes)||!decode(f[1],parsed.source_url)||!decode(f[2],parsed.entity_tag)){
  error="Invalid resume manifest metadata.";return false;
 }
 while(std::getline(in,line)){
  if(line.empty())continue;if(line.size()>256||parsed.chunks.size()>=4096){error="Resume manifest exceeds safety limits.";return false;}
  std::uint64_t first=0,last=0,done=0;
  if(!split(line,f)||f.size()!=3||!number(f[0],first)||!number(f[1],last)||!number(f[2],done)||done>1){error="Invalid resume chunk record.";return false;}
  parsed.chunks.push_back({{first,last},done==1});
 }
 if(in.bad()){error="Failed while reading resume manifest.";return false;}
 if(!validate_resume_manifest(parsed,error))return false;
 output=std::move(parsed);error.clear();return true;
}
}
