#include "pkgmaster/downloader/http_probe.hpp"
#include "pkgmaster/core/url_policy.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#ifdef PKGMASTER_HAS_CURL
#include <curl/curl.h>
#if defined(__unix__) || defined(__APPLE__)
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#endif

namespace pkgmaster {
#ifdef PKGMASTER_HAS_CURL
namespace {
struct Headers {
    std::string etag;
    std::string content_range;
    std::string accept_ranges;
    std::uint64_t content_length{};
    bool has_content_length{false};
    bool invalid_length{false};
};
std::string trim(std::string s) {
    const auto first=s.find_first_not_of(" \t\r\n");
    if(first==std::string::npos) return {};
    const auto last=s.find_last_not_of(" \t\r\n");
    return s.substr(first,last-first+1);
}
size_t header_cb(char* data,size_t size,size_t count,void* opaque) {
    if(size && count>std::numeric_limits<size_t>::max()/size) return 0;
    const size_t n=size*count;
    auto* h=static_cast<Headers*>(opaque);
    std::string line(data,n);
    const auto colon=line.find(':');
    if(colon==std::string::npos) return n;
    std::string key=line.substr(0,colon);
    std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    const std::string value=trim(line.substr(colon+1));
    if(key=="etag") h->etag=value;
    else if(key=="content-range") h->content_range=value;
    else if(key=="accept-ranges") h->accept_ranges=value;
    else if(key=="content-length") {
        char* end=nullptr;
        const auto v=std::strtoull(value.c_str(),&end,10);
        if(value.empty() || !end || *end!='\0') h->invalid_length=true;
        else {h->content_length=v;h->has_content_length=true;}
    }
    return n;
}
size_t discard_body(char*,size_t size,size_t count,void* opaque) {
    if(size && count>std::numeric_limits<size_t>::max()/size) return 0;
    *static_cast<std::size_t*>(opaque)+=size*count;
    return size*count;
}
bool parse_range(const std::string& s,std::uint64_t total) {
    if(s.rfind("bytes 0-0/",0)!=0 || s.size()<=10 || s.find('*')!=std::string::npos) return false;
    char* end=nullptr;
    const auto parsed=std::strtoull(s.c_str()+10,&end,10);
    return end && *end=='\0' && parsed==total && total>0;
}
#if defined(__unix__) || defined(__APPLE__)
bool public_ip(const sockaddr* a) {
    if(!a) return false;
    if(a->sa_family==AF_INET) {
        auto* v=reinterpret_cast<const sockaddr_in*>(a); auto ip=ntohl(v->sin_addr.s_addr);
        unsigned x=ip>>24,y=(ip>>16)&255;
        return !(x==0||x==10||x==127||x>=224||(x==169&&y==254)||(x==172&&y>=16&&y<=31)||(x==192&&y==168)||(x==100&&y>=64&&y<=127)||(x==192&&y==0)||(x==198&&(y==18||y==19)));
    }
    if(a->sa_family==AF_INET6) {
        auto* v=reinterpret_cast<const sockaddr_in6*>(a); const auto* b=v->sin6_addr.s6_addr;
        if(IN6_IS_ADDR_UNSPECIFIED(&v->sin6_addr)||IN6_IS_ADDR_LOOPBACK(&v->sin6_addr)||IN6_IS_ADDR_LINKLOCAL(&v->sin6_addr)||IN6_IS_ADDR_MULTICAST(&v->sin6_addr)||(b[0]&0xfe)==0xfc) return false;
        if(IN6_IS_ADDR_V4MAPPED(&v->sin6_addr)) {sockaddr_in m{};m.sin_family=AF_INET;std::memcpy(&m.sin_addr,b+12,4);return public_ip(reinterpret_cast<const sockaddr*>(&m));}
        return true;
    }
    return false;
}
curl_socket_t open_socket(void*,curlsocktype purpose,curl_sockaddr* a) {
    if(purpose!=CURLSOCKTYPE_IPCXN||!public_ip(reinterpret_cast<const sockaddr*>(&a->addr))) return CURL_SOCKET_BAD;
    return ::socket(a->family,a->socktype,a->protocol);
}
#endif
CURL* make_curl(const std::string& url,long timeout,Headers* headers) {
    CURL* c=curl_easy_init(); if(!c) return nullptr;
    curl_easy_setopt(c,CURLOPT_URL,url.c_str());
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"http,https");
#else
    curl_easy_setopt(c,CURLOPT_PROTOCOLS,CURLPROTO_HTTP|CURLPROTO_HTTPS);
#endif
    curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(c,CURLOPT_PROXY,"");
    curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT_MS,timeout);curl_easy_setopt(c,CURLOPT_TIMEOUT,30L);
    curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);
    curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);curl_easy_setopt(c,CURLOPT_USERAGENT,"PKGMaster/0.1");
    curl_easy_setopt(c,CURLOPT_HEADERFUNCTION,header_cb);curl_easy_setopt(c,CURLOPT_HEADERDATA,headers);
#if defined(__unix__) || defined(__APPLE__)
    curl_easy_setopt(c,CURLOPT_OPENSOCKETFUNCTION,open_socket);
#endif
    return c;
}
bool strong_etag(const std::string& s) {
    if(s.size()<2||s.size()>8192||s.rfind("W/",0)==0||s.front()!='"'||s.back()!='"') return false;
    for(unsigned char c:s) if(c<0x21||c==0x7f) return false;
    return true;
}
}
#endif
HttpResourceProbe probe_http_resource(const std::string& url,long timeout) {
    HttpResourceProbe out;
    std::string why;
    if(!is_allowed_download_url(url,why)){out.error=why;return out;}
    if(timeout<=0){out.error="Connection timeout must be positive.";return out;}
#ifndef PKGMASTER_HAS_CURL
    out.error="This build has no libcurl HTTP transport.";return out;
#else
    static const CURLcode init=curl_global_init(CURL_GLOBAL_DEFAULT);
    if(init!=CURLE_OK){out.error="libcurl initialization failed.";return out;}
    Headers head;
    CURL* raw=make_curl(url,timeout,&head);
    if(!raw){out.error="libcurl could not initialize.";return out;}
    curl_easy_setopt(raw,CURLOPT_NOBODY,1L);
    CURLcode code=curl_easy_perform(raw);
    curl_easy_getinfo(raw,CURLINFO_RESPONSE_CODE,&out.http_status);
    curl_off_t length=-1;curl_easy_getinfo(raw,CURLINFO_CONTENT_LENGTH_DOWNLOAD_T,&length);
    curl_easy_cleanup(raw);
    if(code!=CURLE_OK){out.error=curl_easy_strerror(code);return out;}
    if(out.http_status>=300&&out.http_status<400){out.error="Redirect refused during resource probe.";return out;}
    if(out.http_status<200||out.http_status>=300){out.error="HEAD returned HTTP "+std::to_string(out.http_status)+".";return out;}
    if(head.invalid_length){out.error="Server returned an invalid Content-Length.";return out;}
    if(length>=0) out.content_length=static_cast<std::uint64_t>(length);
    else if(head.has_content_length) out.content_length=head.content_length;
    if(!out.content_length){out.error="Server did not provide a usable non-zero object length.";return out;}
    out.reachable=true;
    out.strong_etag=strong_etag(head.etag)?head.etag:std::string{};
    Headers range_headers;std::size_t body_bytes=0;
    raw=make_curl(url,timeout,&range_headers);
    if(!raw){out.error="Could not initialize range validation.";return out;}
    curl_easy_setopt(raw,CURLOPT_RANGE,"0-0");
    curl_easy_setopt(raw,CURLOPT_WRITEFUNCTION,discard_body);
    curl_easy_setopt(raw,CURLOPT_WRITEDATA,&body_bytes);
    struct curl_slist* request_headers=nullptr;
    if(!out.strong_etag.empty()) {const std::string match="If-Match: "+out.strong_etag;request_headers=curl_slist_append(request_headers,match.c_str());curl_easy_setopt(raw,CURLOPT_HTTPHEADER,request_headers);}
    code=curl_easy_perform(raw);long status=0;curl_easy_getinfo(raw,CURLINFO_RESPONSE_CODE,&status);
    curl_easy_cleanup(raw);if(request_headers)curl_slist_free_all(request_headers);
    if(code==CURLE_OK&&status==206&&body_bytes==1&&parse_range(range_headers.content_range,out.content_length)) {
        const bool same_tag=out.strong_etag.empty()||range_headers.etag==out.strong_etag;
        if(same_tag) out.range_supported=true;
        else out.error="Entity tag changed between HEAD and range validation.";
    } else if(status>=300&&status<400) out.error="Redirect refused during range validation.";
    else out.error="Server did not pass the one-byte HTTP range validation.";
    out.resumable=out.range_supported&&!out.strong_etag.empty();
    return out;
#endif
}
}
