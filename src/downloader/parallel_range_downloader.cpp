#include "pkgmaster/downloader/parallel_range_downloader.hpp"
#include "pkgmaster/downloader/http_probe.hpp"
#include "pkgmaster/downloader/range_file_assembler.hpp"
#include "pkgmaster/downloader/range_plan.hpp"
#include "pkgmaster/downloader/resume_manifest.hpp"
#include "pkgmaster/core/url_policy.hpp"
#include "pkgmaster/core/crc32.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>
#include <algorithm>
#include <cctype>
#ifdef PKGMASTER_HAS_CURL
#include <curl/curl.h>
#if defined(__unix__) || defined(__APPLE__)
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#endif

namespace pkgmaster {
#ifdef PKGMASTER_HAS_CURL
namespace {
struct ResponseHeaders { std::string content_range, etag; };
struct Body {
    std::vector<char> bytes;
    std::uint64_t limit{};
    bool oversized{false};
    const std::atomic_bool* cancel{};
    const std::atomic_bool* failed{};
};
std::string trim(const std::string& v) {
    const auto a=v.find_first_not_of(" \t\r
");
    if(a==std::string::npos)return {};
    return v.substr(a,v.find_last_not_of(" \t\r
")-a+1);
}
size_t header_cb(char* p,size_t size,size_t count,void* opaque) {
    if(size&&count>std::numeric_limits<size_t>::max()/size)return 0;
    const size_t n=size*count;std::string line(p,n);auto* h=static_cast<ResponseHeaders*>(opaque);
    const auto colon=line.find(':');if(colon==std::string::npos)return n;
    std::string key=line.substr(0,colon);
    std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    const auto value=trim(line.substr(colon+1));
    if(key=="content-range")h->content_range=value;
    if(key=="etag")h->etag=value;
    return n;
}
size_t body_cb(char* p,size_t size,size_t count,void* opaque) {
    if(size&&count>std::numeric_limits<size_t>::max()/size)return 0;
    const size_t n=size*count;auto* b=static_cast<Body*>(opaque);
    if(n>b->limit-b->bytes.size()){b->oversized=true;return 0;}
    b->bytes.insert(b->bytes.end(),p,p+n);return n;
}
int transfer_progress(void* opaque,curl_off_t,curl_off_t,curl_off_t,curl_off_t) {
    auto* b=static_cast<Body*>(opaque);
    return (b->cancel&&b->cancel->load())||(b->failed&&b->failed->load())?1:0;
}
#if defined(__unix__) || defined(__APPLE__)
bool public_ip(const sockaddr* a) {
    if(!a)return false;
    if(a->sa_family==AF_INET){const auto ip=ntohl(reinterpret_cast<const sockaddr_in*>(a)->sin_addr.s_addr);const unsigned x=ip>>24,y=(ip>>16)&255;
        return !(x==0||x==10||x==127||x>=224||(x==169&&y==254)||(x==172&&y>=16&&y<=31)||(x==192&&y==168)||(x==100&&y>=64&&y<=127)||(x==192&&y==0)||(x==198&&(y==18||y==19)));}
    if(a->sa_family==AF_INET6){const auto* v=&reinterpret_cast<const sockaddr_in6*>(a)->sin6_addr;const auto* b=v->s6_addr;
        if(IN6_IS_ADDR_UNSPECIFIED(v)||IN6_IS_ADDR_LOOPBACK(v)||IN6_IS_ADDR_LINKLOCAL(v)||IN6_IS_ADDR_MULTICAST(v)||(b[0]&0xfe)==0xfc)return false;
        if(IN6_IS_ADDR_V4MAPPED(v)){sockaddr_in m{};m.sin_family=AF_INET;std::memcpy(&m.sin_addr,b+12,4);return public_ip(reinterpret_cast<const sockaddr*>(&m));}return true;}
    return false;
}
curl_socket_t open_socket(void*,curlsocktype purpose,curl_sockaddr* a){if(purpose!=CURLSOCKTYPE_IPCXN||!public_ip(reinterpret_cast<const sockaddr*>(&a->addr)))return CURL_SOCKET_BAD;return ::socket(a->family,a->socktype,a->protocol);}
#endif
CURL* create_handle(const ParallelRangeOptions& o,const ByteRange& range,
                    const std::string& etag,ResponseHeaders& headers,Body& body) {
    CURL* c=curl_easy_init();if(!c)return nullptr;
    curl_easy_setopt(c,CURLOPT_URL,o.transfer.url.c_str());
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"http,https");
#else
    curl_easy_setopt(c,CURLOPT_PROTOCOLS,CURLPROTO_HTTP|CURLPROTO_HTTPS);
#endif
    curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(c,CURLOPT_PROXY,"");
    curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT_MS,o.transfer.connect_timeout_ms);
    curl_easy_setopt(c,CURLOPT_TIMEOUT,o.transfer.total_timeout_seconds);
    if(o.transfer.maximum_bytes_per_second) {
        const auto per_connection=std::max<std::uint64_t>(1,o.transfer.maximum_bytes_per_second/o.connections);
        curl_easy_setopt(c,CURLOPT_MAX_RECV_SPEED_LARGE,static_cast<curl_off_t>(per_connection));
    }
    curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);
    curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);curl_easy_setopt(c,CURLOPT_USERAGENT,"PKGMaster/0.1");
    const std::string interval=std::to_string(range.first)+"-"+std::to_string(range.last);
    curl_easy_setopt(c,CURLOPT_RANGE,interval.c_str());
    curl_easy_setopt(c,CURLOPT_HEADERFUNCTION,header_cb);curl_easy_setopt(c,CURLOPT_HEADERDATA,&headers);
    curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,body_cb);curl_easy_setopt(c,CURLOPT_WRITEDATA,&body);
    curl_easy_setopt(c,CURLOPT_XFERINFOFUNCTION,transfer_progress);curl_easy_setopt(c,CURLOPT_XFERINFODATA,&body);
    curl_easy_setopt(c,CURLOPT_NOPROGRESS,0L);
#if defined(__unix__) || defined(__APPLE__)
    curl_easy_setopt(c,CURLOPT_OPENSOCKETFUNCTION,open_socket);
#endif
    const std::string match="If-Match: "+etag;
    auto* list=curl_slist_append(nullptr,match.c_str());
    if(!list){curl_easy_cleanup(c);return nullptr;}
    curl_easy_setopt(c,CURLOPT_HTTPHEADER,list);
    curl_easy_setopt(c,CURLOPT_PRIVATE,list);
    return c;
}
}
#endif

ParallelRangeResult download_http_ranges(const ParallelRangeOptions& o) {
    ParallelRangeResult out;
    const auto& t=o.transfer;
    if(t.url.empty()||t.destination_path.empty()||!t.maximum_bytes||
       t.connect_timeout_ms<=0||t.total_timeout_seconds<0||o.connections<2||
       o.connections>8||!o.minimum_part_bytes||
       t.maximum_bytes_per_second>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())){out.error="Invalid parallel download options.";return out;}
    std::string policy_error;
    if(!is_allowed_download_url(t.url,policy_error)){out.error=policy_error;return out;}
#ifndef PKGMASTER_HAS_CURL
    out.status=DownloadStatus::Unsupported;out.error="This build has no libcurl HTTP transport.";return out;
#else
    if(t.cancel&&t.cancel->load()){out.status=DownloadStatus::Cancelled;out.error="Cancelled before starting.";return out;}
    static const CURLcode init=curl_global_init(CURL_GLOBAL_DEFAULT);
    if(init!=CURLE_OK){out.error="libcurl initialization failed.";return out;}
    const auto probe=probe_http_resource(t.url,t.connect_timeout_ms);
    if(!probe.reachable||!probe.range_supported){out.error=probe.error.empty()?"Remote server does not support validated byte ranges.":probe.error;return out;}
    if(!probe.resumable){out.error="Parallel mode requires a strong ETag so every range can be bound to the same object version.";return out;}
    if(probe.content_length>t.maximum_bytes){out.error="Remote object exceeds configured maximum size.";return out;}

    const std::string manifest_path=t.destination_path+".resume";
    const std::string staging_path=t.destination_path+".part";
    const bool has_manifest=std::ifstream(manifest_path,std::ios::binary).good();
    const bool has_staging=std::ifstream(staging_path,std::ios::binary).good();
    if(has_manifest!=has_staging){out.error="Incomplete resume state: both manifest and staging file must exist. Preserve them for inspection.";return out;}

    ResumeManifest manifest;
    RangePlan plan;
    RangeFileAssembler assembler;
    if(has_manifest){
        if(!load_resume_manifest(manifest_path,manifest,out.error))return out;
        if(!resume_identity_matches(manifest,t.url,probe.strong_etag,probe.content_length)){
            out.error="Remote object identity changed; refusing to reuse previous partial bytes.";return out;
        }
        plan.total_bytes=manifest.total_bytes;
        std::vector<bool> completed;
        completed.reserve(manifest.chunks.size());
        for(const auto& chunk:manifest.chunks){plan.parts.push_back(chunk.range);completed.push_back(chunk.complete);}
        if(!assembler.open_existing(t.destination_path,plan,t.maximum_bytes,t.overwrite,completed,out.error))return out;
        for(std::size_t i=0;i<manifest.chunks.size();++i) {
            if(manifest.chunks[i].complete) {
                const bool verified=manifest.chunks[i].checksum_valid &&
                    assembler.verify_chunk_checksum(i,manifest.chunks[i].checksum,out.error);
                if(!verified) {
                    std::string reset_error;
                    if(!assembler.invalidate_chunk(i,reset_error)){out.error=reset_error;return out;}
                    manifest.chunks[i].complete=false;
                    manifest.chunks[i].checksum_valid=false;
                    manifest.chunks[i].checksum=0;
                    if(!save_resume_manifest(manifest_path,manifest,out.error))return out;
                }
            }
        }
    } else {
        plan=make_range_plan(probe.content_length,o.connections,o.minimum_part_bytes);
        if(plan.parts.size()<2){out.error="Object is too small to benefit from parallel ranges.";return out;}
        manifest.source_url=t.url;manifest.entity_tag=probe.strong_etag;manifest.total_bytes=probe.content_length;
        for(const auto& range:plan.parts)manifest.chunks.push_back({range,false});
        if(!save_resume_manifest(manifest_path,manifest,out.error))return out;
        if(!assembler.create(t.destination_path,plan,t.maximum_bytes,t.overwrite,out.error)){
            std::remove(manifest_path.c_str());return out;
        }
    }

    std::vector<std::size_t> pending;
    std::uint64_t already_received=0;
    for(std::size_t i=0;i<manifest.chunks.size();++i){
        if(manifest.chunks[i].complete)already_received+=manifest.chunks[i].range.length();
        else pending.push_back(i);
    }
    std::atomic_size_t next{0};
    std::atomic_uint64_t received{already_received};
    std::atomic_uint completed{0};
    for(const auto& chunk:manifest.chunks)if(chunk.complete)completed.fetch_add(1);
    std::atomic_bool failed{false};
    DownloadSpeedTracker speed_tracker;
    std::mutex error_mutex,progress_mutex,manifest_mutex;
    std::string first_error;
    auto set_error=[&](const std::string& error){failed=true;std::lock_guard<std::mutex> lock(error_mutex);if(first_error.empty())first_error=error;};
    auto worker=[&](){
        while(!failed.load()){
            if(t.cancel&&t.cancel->load())return;
            const auto cursor=next.fetch_add(1);if(cursor>=pending.size())return;
            const auto index=pending[cursor];const auto& range=plan.parts[index];
            if(range.length()>static_cast<std::uint64_t>(std::numeric_limits<size_t>::max())){set_error("Range exceeds addressable memory.");return;}
            ResponseHeaders headers;Body body;body.limit=range.length();body.cancel=t.cancel;body.failed=&failed;
            CURL* c=create_handle(o,range,probe.strong_etag,headers,body);
            if(!c){set_error("Could not initialize a range request.");return;}
            const CURLcode code=curl_easy_perform(c);long status=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&status);
            curl_slist* list=nullptr;curl_easy_getinfo(c,CURLINFO_PRIVATE,&list);curl_easy_cleanup(c);if(list)curl_slist_free_all(list);
            if(t.cancel&&t.cancel->load())return;
            if(failed.load())return;
            std::uint64_t first=0,last=0,total=0;std::string why;
            RangeReply reply;reply.http_status=static_cast<int>(status);reply.body_bytes=body.bytes.size();reply.etag=headers.etag;
            const bool parsed=parse_content_range_header(headers.content_range,first,last,total,why);
            reply.first=first;reply.last=last;reply.total=total;
            bool valid=code==CURLE_OK&&!body.oversized&&parsed&&validate_range_reply(range,reply,probe.content_length,why);
            if(valid&&headers.etag!=probe.strong_etag){valid=false;why="ETag changed or was omitted on a range response.";}
            if(!valid){set_error(why.empty()?(code==CURLE_OK?"Invalid range response.":curl_easy_strerror(code)):why);return;}
            if(!assembler.write_chunk(index,body.bytes.data(),body.bytes.size(),why)){set_error(why);return;}
            {
                std::lock_guard<std::mutex> lock(manifest_mutex);
                const auto checksum=crc32(body.bytes.data(),body.bytes.size());
                if(!mark_resume_chunk_complete(manifest,index,why,checksum,true)||!save_resume_manifest(manifest_path,manifest,why)){set_error(why);return;}
            }
            const auto now=received.fetch_add(body.bytes.size())+body.bytes.size();completed.fetch_add(1);
            if(t.progress||t.metrics){try{
                std::lock_guard<std::mutex> lock(progress_mutex);
                if(t.progress)t.progress(now,probe.content_length);
                if(t.metrics)t.metrics(speed_tracker.update(now,probe.content_length));
            }catch(...){set_error("Progress callback failed.");return;}}
        }
    };
    std::vector<std::thread> threads;threads.reserve(o.connections);
    try{for(unsigned i=0;i<o.connections;++i)threads.emplace_back(worker);}
    catch(...){set_error("Could not start all download workers.");}
    for(auto& thread:threads)if(thread.joinable())thread.join();
    out.received_bytes=received.load();out.expected_bytes=probe.content_length;out.completed_parts=completed.load();
    if(t.cancel&&t.cancel->load()){out.status=DownloadStatus::Cancelled;out.error="Download cancelled. Completed ranges and manifest were retained for restart.";}
    else if(failed.load()){out.error=first_error.empty()?"Parallel range download failed.":first_error;}
    else if(completed.load()!=plan.parts.size()){out.error="Parallel range download ended before every part completed.";}
    else if(!assembler.finalize(out.error)){}
    else{std::remove(manifest_path.c_str());out.status=DownloadStatus::Completed;return out;}
    return out;
#endif
}
}
