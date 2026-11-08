#include "pkgmaster/downloader/parallel_range_downloader.hpp"
#include "pkgmaster/downloader/http_probe.hpp"
#include "pkgmaster/downloader/range_file_assembler.hpp"
#include "pkgmaster/downloader/range_plan.hpp"
#include "pkgmaster/core/url_policy.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>
#include <algorithm>
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
struct ResponseHeaders { std::string content_range, etag; bool invalid{false}; };
struct Body { std::vector<char> bytes; std::uint64_t limit{}; bool oversized{false}; };
std::string trim(const std::string& v) {
    const auto a=v.find_first_not_of(" \t\r\n");
    if(a==std::string::npos) return {};
    return v.substr(a,v.find_last_not_of(" \t\r\n")-a+1);
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
    curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);
    curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);curl_easy_setopt(c,CURLOPT_USERAGENT,"PKGMaster/0.1");
    curl_easy_setopt(c,CURLOPT_RANGE,(std::to_string(range.first)+"-"+std::to_string(range.last)).c_str());
    curl_easy_setopt(c,CURLOPT_HEADERFUNCTION,header_cb);curl_easy_setopt(c,CURLOPT_HEADERDATA,&headers);
    curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,body_cb);curl_easy_setopt(c,CURLOPT_WRITEDATA,&body);
#if defined(__unix__) || defined(__APPLE__)
    curl_easy_setopt(c,CURLOPT_OPENSOCKETFUNCTION,open_socket);
#endif
    const std::string match="If-Match: "+etag;
    auto* list=curl_slist_append(nullptr,match.c_str());curl_easy_setopt(c,CURLOPT_HTTPHEADER,list);
    // The list is retained by the handle and freed by the caller through CURLINFO_PRIVATE.
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
       o.connections>8||!o.minimum_part_bytes){out.error="Invalid parallel download options.";return out;}
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
    const auto plan=make_range_plan(probe.content_length,o.connections,o.minimum_part_bytes);
    if(plan.parts.size()<2){out.error="Object is too small to benefit from parallel ranges.";return out;}
    RangeFileAssembler assembler;
    if(!assembler.create(t.destination_path,plan,t.maximum_bytes,t.overwrite,out.error))return out;
    std::atomic_size_t next{0};std::atomic_uint64_t received{0};std::atomic_uint completed{0};
    std::atomic_bool failed{false};std::mutex error_mutex,progress_mutex;
    std::string first_error;
    auto worker=[&](){
        while(!failed.load()){
            if(t.cancel&&t.cancel->load())return;
            const auto i=next.fetch_add(1);if(i>=plan.parts.size())return;
            const auto& range=plan.parts[i];
            if(range.length()>static_cast<std::uint64_t>(std::numeric_limits<size_t>::max())){failed=true;std::lock_guard<std::mutex> l(error_mutex);if(first_error.empty())first_error="Range exceeds addressable memory.";return;}
            ResponseHeaders headers;Body body;body.limit=range.length();
            CURL* c=create_handle(o,range,probe.strong_etag,headers,body);
            if(!c){failed=true;std::lock_guard<std::mutex> l(error_mutex);if(first_error.empty())first_error="Could not initialize a range request.";return;}
            const CURLcode code=curl_easy_perform(c);long status=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&status);
            curl_slist* list=nullptr;curl_easy_getinfo(c,CURLINFO_PRIVATE,&list);curl_easy_cleanup(c);if(list)curl_slist_free_all(list);
            std::uint64_t first=0,last=0,total=0;std::string why;
            RangeReply reply;reply.http_status=static_cast<int>(status);reply.body_bytes=body.bytes.size();reply.etag=headers.etag;
            const bool parsed=parse_content_range_header(headers.content_range,first,last,total,why);
            reply.first=first;reply.last=last;reply.total=total;
            bool valid=code==CURLE_OK&&!body.oversized&&parsed&&validate_range_reply(range,reply,probe.content_length,why);
            if(valid&&headers.etag!=probe.strong_etag){valid=false;why="ETag changed or was omitted on a range response.";}
            if(!valid){failed=true;std::lock_guard<std::mutex> l(error_mutex);if(first_error.empty())first_error=why.empty()?(code==CURLE_OK?"Invalid range response.":curl_easy_strerror(code)):why;return;}
            if(!assembler.write_chunk(i,body.bytes.data(),body.bytes.size(),why)){failed=true;std::lock_guard<std::mutex> l(error_mutex);if(first_error.empty())first_error=why;return;}
            const auto now=received.fetch_add(body.bytes.size())+body.bytes.size();completed.fetch_add(1);
            if(t.progress){try{std::lock_guard<std::mutex> l(progress_mutex);t.progress(now,probe.content_length);}catch(...){failed=true;std::lock_guard<std::mutex> e(error_mutex);if(first_error.empty())first_error="Progress callback failed.";return;}}
        }
    };
    std::vector<std::thread> threads;threads.reserve(o.connections);
    try{for(unsigned i=0;i<o.connections;++i)threads.emplace_back(worker);}
    catch(...){failed=true;{std::lock_guard<std::mutex> l(error_mutex);first_error="Could not start all download workers.";} }
    for(auto& thread:threads)if(thread.joinable())thread.join();
    out.received_bytes=received.load();out.expected_bytes=probe.content_length;out.completed_parts=completed.load();
    if(t.cancel&&t.cancel->load()){out.status=DownloadStatus::Cancelled;out.error="Parallel download cancelled; staging data retained for future recovery.";}
    else if(failed.load()){out.error=first_error.empty()?"Parallel range download failed.":first_error;}
    else if(completed.load()!=plan.parts.size()){out.error="Parallel range download ended before every part completed.";}
    else if(!assembler.finalize(out.error)){}
    else{out.status=DownloadStatus::Completed;return out;}
    return out;
#endif
}
}
