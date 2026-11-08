#include "pkgmaster/core/sha256.hpp"
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace pkgmaster {
namespace {
constexpr std::array<std::uint32_t,64> K{{
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2}};
constexpr std::uint32_t rotr(std::uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
class Sha256 {
public:
    void update(const unsigned char* data,std::size_t len){
        for(std::size_t i=0;i<len;++i){buffer_[used_++]=data[i];if(used_==64){transform();bitlen_+=512;used_=0;}}
    }
    std::string finish(){
        bitlen_+=static_cast<std::uint64_t>(used_)*8;
        buffer_[used_++]=0x80;
        if(used_>56){while(used_<64)buffer_[used_++]=0;transform();used_=0;}
        while(used_<56)buffer_[used_++]=0;
        for(int i=7;i>=0;--i)buffer_[used_++]=static_cast<unsigned char>(bitlen_>>(i*8));
        transform();
        std::ostringstream out;out<<std::hex<<std::setfill('0');
        for(auto word:state_)out<<std::setw(8)<<word;
        return out.str();
    }
private:
    void transform(){
        std::uint32_t w[64]{};
        for(unsigned i=0;i<16;++i)w[i]=(std::uint32_t(buffer_[4*i])<<24)|(std::uint32_t(buffer_[4*i+1])<<16)|(std::uint32_t(buffer_[4*i+2])<<8)|buffer_[4*i+3];
        for(unsigned i=16;i<64;++i){const auto s0=rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3);const auto s1=rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}
        auto a=state_[0],b=state_[1],c=state_[2],d=state_[3],e=state_[4],f=state_[5],g=state_[6],h=state_[7];
        for(unsigned i=0;i<64;++i){const auto s1=rotr(e,6)^rotr(e,11)^rotr(e,25);const auto ch=(e&f)^(~e&g);const auto t1=h+s1+ch+K[i]+w[i];const auto s0=rotr(a,2)^rotr(a,13)^rotr(a,22);const auto maj=(a&b)^(a&c)^(b&c);const auto t2=s0+maj;h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
        state_[0]+=a;state_[1]+=b;state_[2]+=c;state_[3]+=d;state_[4]+=e;state_[5]+=f;state_[6]+=g;state_[7]+=h;
    }
    std::array<std::uint32_t,8> state_{{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
    std::array<unsigned char,64> buffer_{};std::size_t used_{0};std::uint64_t bitlen_{0};
};
}
std::string sha256_hex(const void* data,std::size_t size){
    Sha256 hash;hash.update(static_cast<const unsigned char*>(data),size);return hash.finish();
}
bool is_sha256_hex(const std::string& value) noexcept{
    if(value.size()!=64)return false;
    for(unsigned char c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;
    return true;
}
bool sha256_file(const std::string& path,std::string& digest,std::string& error){
    std::ifstream in(path,std::ios::binary);if(!in){error="Could not open file for SHA-256 verification.";return false;}
    Sha256 hash;std::array<unsigned char,65536> buffer{};
    while(in){in.read(reinterpret_cast<char*>(buffer.data()),static_cast<std::streamsize>(buffer.size()));const auto n=in.gcount();if(n>0)hash.update(buffer.data(),static_cast<std::size_t>(n));}
    if(in.bad()){error="Could not read file for SHA-256 verification.";return false;}
    digest=hash.finish();error.clear();return true;
}
}
