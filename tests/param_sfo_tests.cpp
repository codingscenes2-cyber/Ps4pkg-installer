#include "pkgmaster/package/param_sfo.hpp"
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {
void put16(std::vector<std::uint8_t>& b,std::size_t p,std::uint16_t v){b[p]=v&255;b[p+1]=(v>>8)&255;}
void put32(std::vector<std::uint8_t>& b,std::size_t p,std::uint32_t v){for(unsigned i=0;i<4;++i)b[p+i]=static_cast<std::uint8_t>((v>>(8*i))&255);}
}
int main(){
    const std::vector<std::pair<std::string,std::string>> fields={
        {"TITLE","PKGMaster Test"},{"TITLE_ID","CUSA12345"},{"APP_VER","01.23"},
        {"CATEGORY","gd"},{"SYSTEM_VER","07000000"}};
    const std::size_t index=20,key_table=index+fields.size()*16;
    std::size_t keys=0,data=0;for(const auto& f:fields){keys+=f.first.size()+1;data+=f.second.size()+1;}
    const std::size_t data_table=key_table+keys;
    std::vector<std::uint8_t> bytes(data_table+data,0);
    bytes[0]=0;bytes[1]='P';bytes[2]='S';bytes[3]='F';
    put32(bytes,4,0x00000101);put32(bytes,8,key_table);put32(bytes,12,data_table);put32(bytes,16,fields.size());
    std::size_t key_pos=0,data_pos=0;
    for(std::size_t i=0;i<fields.size();++i){
        const auto p=index+i*16;put16(bytes,p,static_cast<std::uint16_t>(key_pos));put16(bytes,p+2,0x0204);
        put32(bytes,p+4,fields[i].second.size()+1);put32(bytes,p+8,fields[i].second.size()+1);put32(bytes,p+12,data_pos);
        std::copy(fields[i].first.begin(),fields[i].first.end(),bytes.begin()+key_table+key_pos);
        std::copy(fields[i].second.begin(),fields[i].second.end(),bytes.begin()+data_table+data_pos);
        key_pos+=fields[i].first.size()+1;data_pos+=fields[i].second.size()+1;
    }
    pkgmaster::PackageMetadata metadata;std::string error;
    assert(pkgmaster::parse_param_sfo(bytes,metadata,error));
    assert(metadata.title=="PKGMaster Test"&&metadata.title_id=="CUSA12345");
    assert(metadata.app_version=="01.23"&&metadata.category=="gd"&&metadata.system_version=="07000000");
    auto invalid=bytes;invalid[0]='X';
    assert(!pkgmaster::parse_param_sfo(invalid,metadata,error));
    invalid=bytes;put32(invalid,16,0xffffffffu);
    assert(!pkgmaster::parse_param_sfo(invalid,metadata,error));
    invalid=bytes;put32(invalid,8,0xfffffff0u);
    assert(!pkgmaster::parse_param_sfo(invalid,metadata,error));
    return 0;
}
