#include "pkgmaster/platform/capabilities.hpp"
#include <iomanip>
#include <sstream>
namespace pkgmaster {
bool has_capability(const RuntimeInfo& r, Capability c) { const auto m=static_cast<std::uint32_t>(c); return m==0 || (r.available&m)==m; }
FeatureDecision evaluate_feature(const RuntimeInfo& r, Capability c) {
 if(c==Capability::None) return {true,"No platform capability required."};
 if(has_capability(r,c)) return {true,"Required runtime capability is present."};
 if(!r.firmware_detected&&!r.goldhen_detected) return {false,"Runtime has not reported capabilities; feature is disabled safely."};
 return {false,"Required platform capability is unavailable in this environment."};
}
std::string describe_runtime(const RuntimeInfo& r) {
 std::ostringstream o;
 if(r.firmware_detected) o<<"Firmware "<<r.firmware_major<<"."<<r.firmware_minor<<"."<<r.firmware_patch; else o<<"Firmware unknown";
 o<<"; GoldHEN "<<(r.goldhen_detected&&!r.goldhen_version.empty()?r.goldhen_version:"unknown");
 o<<"; model "<<(r.console_model.empty()?"unknown":r.console_model)<<"; capability mask 0x"<<std::hex<<r.available;
 return o.str();
}
bool parse_firmware_version(const std::string& s,int& a,int& b,int& c) {
 a=b=c=0; if(s.empty()) return false; std::istringstream in(s); char dot=0;
 if(!(in>>a>>dot>>b)||dot!='.') return false;
 if(in.peek()=='.'){in.get(dot);if(!(in>>c))return false;}
 return in.peek()==std::char_traits<char>::eof()&&a>=0&&b>=0&&c>=0;
}
}
