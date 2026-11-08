#include "pkgmaster/core/url_policy.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

namespace pkgmaster {
namespace {
bool private_ipv4(const std::string& host) {
    std::istringstream in(host); std::vector<unsigned> octets; std::string part;
    while (std::getline(in, part, '.')) {
        if (part.empty() || part.size() > 3) return false;
        unsigned value=0;
        for (unsigned char c:part) { if (!std::isdigit(c)) return false; value=value*10+(c-'0'); }
        if (value>255) return false;
        octets.push_back(value);
    }
    if (octets.size()!=4) return false;
    const auto a=octets[0], b=octets[1];
    return a==0 || a==10 || a==127 || (a==169&&b==254) ||
           (a==172&&b>=16&&b<=31) || (a==192&&b==168) ||
           (a==100&&b>=64&&b<=127) || a>=224;
}
bool private_ipv6_literal(const std::string& host) {
    if (host.size()<2 || host.front()!='[' || host.back()!=']') return false;
    std::string ip=host.substr(1,host.size()-2);
    std::transform(ip.begin(),ip.end(),ip.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return ip=="::" || ip=="::1" || ip.rfind("fc",0)==0 ||
           ip.rfind("fd",0)==0 || ip.rfind("fe8",0)==0 ||
           ip.rfind("fe9",0)==0 || ip.rfind("fea",0)==0 ||
           ip.rfind("feb",0)==0 || ip.rfind("ff",0)==0;
}
}
bool is_allowed_download_url(const std::string& url, std::string& reason) {
    const auto colon=url.find("://");
    if(colon==std::string::npos){reason="URL must use HTTP or HTTPS.";return false;}
    std::string scheme=url.substr(0,colon);
    std::transform(scheme.begin(),scheme.end(),scheme.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    if(scheme!="http"&&scheme!="https"){reason="Only HTTP and HTTPS links are accepted.";return false;}
    const auto start=colon+3;
    const auto end=url.find_first_of("/:?#",start);
    std::string authority=url.substr(start,end-start);
    if(authority.empty()||authority.find('@')!=std::string::npos){reason="URL host is missing or contains credentials.";return false;}
    std::string host=authority;
    if(host.front()!='['){const auto port=host.find(':');if(port!=std::string::npos)host=host.substr(0,port);}
    std::transform(host.begin(),host.end(),host.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    if(host=="localhost"||host.size()>0&&host.back()=='.'&&host=="localhost."||
       private_ipv4(host)||private_ipv6_literal(host)){
        reason="Local, private or non-public destinations are blocked by default.";return false;
    }
    reason.clear();return true;
}
}
