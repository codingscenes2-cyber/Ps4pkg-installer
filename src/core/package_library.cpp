#include "pkgmaster/core/package_library.hpp"
#include "pkgmaster/core/sha256.hpp"
#include "pkgmaster/core/url_policy.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <limits>

namespace pkgmaster {
namespace {
std::string hex_encode(const std::string& s){static const char d[]="0123456789abcdef";std::string o;o.reserve(s.size()*2);for(unsigned char c:s){o.push_back(d[c>>4]);o.push_back(d[c&15]);}return o;}
int hex_digit(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
bool hex_decode(const std::string& s,std::string& out){if(s.size()%2||s.size()>2*1024*1024)return false;std::string v;v.reserve(s.size()/2);for(std::size_t i=0;i<s.size();i+=2){int a=hex_digit(s[i]),b=hex_digit(s[i+1]);if(a<0||b<0)return false;v.push_back(static_cast<char>((a<<4)|b));}out=std::move(v);return true;}
bool number(const std::string& s,std::uint64_t& out){if(s.empty())return false;std::uint64_t v=0;for(char c:s){if(c<'0'||c>'9')return false;unsigned d=static_cast<unsigned>(c-'0');if(v>(std::numeric_limits<std::uint64_t>::max()-d)/10)return false;v=v*10+d;}out=v;return true;}
std::vector<std::string> split(const std::string& s){std::vector<std::string> f;std::size_t p=0;for(;;){auto e=s.find('\t',p);f.push_back(s.substr(p,e==std::string::npos?std::string::npos:e-p));if(e==std::string::npos)break;p=e+1;}return f;}
std::string lower(std::string s){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return s;}
bool is_pkg_path(const std::filesystem::path& p){return lower(p.extension().string())==".pkg";}
}
bool PackageLibrary::load(std::string& error){
    std::ifstream in(index_path_,std::ios::binary);if(!in){error="Could not open package library index.";return false;}
    std::string line;if(!std::getline(in,line)||line!="PKGMASTER_LIBRARY\t1"){error="Unsupported or corrupt package library index.";return false;}
    std::vector<PackageRecord> parsed;std::uint64_t next=1;
    while(std::getline(in,line)){
        if(line.empty())continue;if(line.size()>4*1024*1024||parsed.size()>=100000){error="Package library index exceeds safety limits.";return false;}
        auto f=split(line);PackageRecord r;
        if(f.size()!=6||!number(f[0],r.id)||!number(f[1],r.size_bytes)||
           !hex_decode(f[2],r.file_path)||!hex_decode(f[3],r.file_name)||
           !hex_decode(f[4],r.sha256)||!hex_decode(f[5],r.source_url)||
           !r.id||!is_sha256_hex(r.sha256)||!is_pkg_path(r.file_name)){
            error="Invalid package library record.";return false;
        }
        if(!r.source_url.empty()){std::string why;if(!is_allowed_download_url(r.source_url,why)){error="Package record has a disallowed source URL.";return false;}}
        for(const auto& old:parsed)if(old.id==r.id||old.file_path==r.file_path){error="Duplicate package library ID or path.";return false;}
        if(r.id==std::numeric_limits<std::uint64_t>::max()){error="Package library ID overflow.";return false;}
        next=std::max(next,r.id+1);parsed.push_back(std::move(r));
    }
    if(in.bad()){error="Failed while reading package library index.";return false;}
    {std::lock_guard<std::mutex> lock(mutex_);records_=std::move(parsed);next_id_=next;}
    error.clear();return true;
}
bool PackageLibrary::save(std::string& error) const {
    std::vector<PackageRecord> records;
    {std::lock_guard<std::mutex> lock(mutex_);records=records_;}
    if(index_path_.empty()){error="Package library index path is empty.";return false;}
    const auto temp=index_path_+".tmp";std::ofstream out(temp,std::ios::binary|std::ios::trunc);
    if(!out){error="Could not create package library temporary index.";return false;}
    out<<"PKGMASTER_LIBRARY\t1\n";
    for(const auto& r:records)out<<r.id<<'\t'<<r.size_bytes<<'\t'<<hex_encode(r.file_path)<<'\t'<<hex_encode(r.file_name)<<'\t'<<hex_encode(r.sha256)<<'\t'<<hex_encode(r.source_url)<<'\n';
    out.flush();bool ok=static_cast<bool>(out);out.close();ok=ok&&!out.fail();
    if(!ok){std::remove(temp.c_str());error="Could not write package library index.";return false;}
    if(std::rename(temp.c_str(),index_path_.c_str())!=0){std::remove(temp.c_str());error="Could not publish package library index.";return false;}
    error.clear();return true;
}
bool PackageLibrary::add_package(const std::string& path,const std::string& source_url,std::uint64_t& id,std::string& error){
    namespace fs=std::filesystem;std::error_code ec;const fs::path input(path);
    const auto status=fs::symlink_status(input,ec);
    if(ec||fs::is_symlink(status)||!fs::is_regular_file(status)||!is_pkg_path(input)){error="Only regular, non-symlink .pkg files can be added.";return false;}
    const auto absolute=fs::canonical(input,ec);if(ec){error="Could not resolve package file path.";return false;}
    const auto size=fs::file_size(absolute,ec);if(ec||!size){error="Could not determine a non-zero package file size.";return false;}
    std::string digest;if(!sha256_file(absolute.string(),digest,error))return false;
    if(!source_url.empty()&&!is_allowed_download_url(source_url,error))return false;
    PackageRecord record;record.file_path=absolute.string();record.file_name=absolute.filename().string();record.size_bytes=size;record.sha256=digest;record.source_url=source_url;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for(const auto& existing:records_)if(existing.file_path==record.file_path){id=existing.id;error.clear();return true;}
        if(next_id_==std::numeric_limits<std::uint64_t>::max()){error="Package library ID space exhausted.";return false;}
        record.id=next_id_++;records_.push_back(record);id=record.id;
    }
    if(!save(error)){std::lock_guard<std::mutex> lock(mutex_);records_.erase(std::remove_if(records_.begin(),records_.end(),[&](const PackageRecord& r){return r.id==id;}),records_.end());return false;}
    return true;
}
bool PackageLibrary::remove(std::uint64_t id,std::string& error){
    PackageRecord removed;std::size_t index=0;
    {std::lock_guard<std::mutex> lock(mutex_);auto it=std::find_if(records_.begin(),records_.end(),[&](const PackageRecord& r){return r.id==id;});if(it==records_.end()){error="Package library record was not found.";return false;}index=static_cast<std::size_t>(it-records_.begin());removed=*it;records_.erase(it);}
    if(!save(error)){std::lock_guard<std::mutex> lock(mutex_);records_.insert(records_.begin()+static_cast<std::ptrdiff_t>(std::min(index,records_.size())),removed);return false;}
    error.clear();return true;
}
std::vector<PackageRecord> PackageLibrary::snapshot() const {std::lock_guard<std::mutex> lock(mutex_);return records_;}
std::vector<PackageRecord> PackageLibrary::search(const std::string& query) const {
    const auto needle=lower(query);std::vector<PackageRecord> out;std::lock_guard<std::mutex> lock(mutex_);
    for(const auto& r:records_)if(needle.empty()||lower(r.file_name).find(needle)!=std::string::npos||lower(r.file_path).find(needle)!=std::string::npos||lower(r.sha256).find(needle)!=std::string::npos)out.push_back(r);
    return out;
}
bool PackageLibrary::scan_directory(const std::string& root,std::size_t maximum,std::size_t& added,std::string& error){
    namespace fs=std::filesystem;added=0;if(!maximum){error="Scan file limit must be positive.";return false;}
    std::error_code ec;if(!fs::is_directory(root,ec)||ec){error="Package scan root is not a directory.";return false;}
    fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied,ec),end;
    if(ec){error="Could not enumerate package scan root.";return false;}
    std::size_t inspected=0;
    for(;it!=end;it.increment(ec)){
        if(ec){ec.clear();continue;}
        const auto status=it->symlink_status(ec);if(ec){ec.clear();continue;}
        if(fs::is_symlink(status)){if(it->is_directory(ec))it.disable_recursion_pending();continue;}
        if(!fs::is_regular_file(status)||!is_pkg_path(it->path()))continue;
        if(++inspected>maximum){error="Package scan reached its configured file limit.";return false;}
        std::uint64_t id=0;if(add_package(it->path().string(),{},id,error))++added;else return false;
    }
    error.clear();return true;
}
}
