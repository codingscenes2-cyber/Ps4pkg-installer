#include "pkgmaster/package/library_scanner.hpp"
#include <cassert>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#endif
namespace fs = std::filesystem;
static void put16(std::vector<std::uint8_t>& b, std::size_t p, std::uint16_t v) {
    b[p]=static_cast<std::uint8_t>(v); b[p+1]=static_cast<std::uint8_t>(v>>8);
}
static void put32(std::vector<std::uint8_t>& b, std::size_t p, std::uint32_t v) {
    for(int i=0;i<4;++i)b[p+i]=static_cast<std::uint8_t>(v>>(8*i));
}
static std::vector<std::uint8_t> make_sfo() {
    const std::vector<std::pair<std::string,std::string>> fields={
        {"TITLE","Scanner Test"},{"TITLE_ID","CUSA12345"}};
    const std::size_t index=20,key_table=index+fields.size()*16;
    std::size_t keys=0,data=0;
    for(const auto& field:fields){keys+=field.first.size()+1;data+=field.second.size()+1;}
    const std::size_t data_table=key_table+keys;
    std::vector<std::uint8_t> bytes(data_table+data,0);
    bytes[0]=0;bytes[1]='P';bytes[2]='S';bytes[3]='F';
    put32(bytes,4,0x00000101);put32(bytes,8,static_cast<std::uint32_t>(key_table));
    put32(bytes,12,static_cast<std::uint32_t>(data_table));put32(bytes,16,static_cast<std::uint32_t>(fields.size()));
    std::size_t key_pos=0,data_pos=0;
    for(std::size_t i=0;i<fields.size();++i){
        const auto p=index+i*16;
        put16(bytes,p,static_cast<std::uint16_t>(key_pos));put16(bytes,p+2,0x0204);
        put32(bytes,p+4,static_cast<std::uint32_t>(fields[i].second.size()+1));
        put32(bytes,p+8,static_cast<std::uint32_t>(fields[i].second.size()+1));
        put32(bytes,p+12,static_cast<std::uint32_t>(data_pos));
        std::copy(fields[i].first.begin(),fields[i].first.end(),bytes.begin()+key_table+key_pos);
        std::copy(fields[i].second.begin(),fields[i].second.end(),bytes.begin()+data_table+data_pos);
        key_pos+=fields[i].first.size()+1;data_pos+=fields[i].second.size()+1;
    }
    return bytes;
}
static void write_sfo(const fs::path& p, bool valid=true) {
    fs::create_directories(p.parent_path());
    auto bytes=make_sfo(); if(!valid) bytes[1]='X';
    std::ofstream out(p,std::ios::binary); out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
}
int main() {
    const auto base=fs::temp_directory_path()/("pkgmaster-scan-"+std::to_string(
#ifndef _WIN32
        static_cast<unsigned long long>(::getpid())
#else
        987654ULL
#endif
    ));
    fs::remove_all(base);
    write_sfo(base/"titles"/"one"/"sce_sys"/"param.sfo");
    write_sfo(base/"titles"/"two"/"param.sfo");
    write_sfo(base/"titles"/"bad"/"sce_sys"/"param.sfo",false);
    pkgmaster::PackageMetadata direct_metadata; std::string direct_error;
    assert(pkgmaster::parse_param_sfo(make_sfo(),direct_metadata,direct_error));
    std::vector<pkgmaster::LibraryPackage> results;
    std::string error;
    pkgmaster::PackageScanOptions options;
    assert(pkgmaster::scan_package_library({(base/"titles").string()},options,results,error));
    assert(results.size()==2);
    assert(results[0].metadata.title=="Scanner Test");
    assert(results[0].metadata.title_id=="CUSA12345");
    assert(results[0].param_sfo_bytes>0);
    options.max_depth=0;
    assert(pkgmaster::scan_package_library({(base/"titles").string()},options,results,error));
    assert(results.empty());
    options.max_depth=8;
    options.max_directories=1;
    assert(!pkgmaster::scan_package_library({(base/"titles").string()},options,results,error));
    assert(error.find("limit")!=std::string::npos);
#ifndef _WIN32
    fs::create_directories(base/"outside"/"sce_sys");
    write_sfo(base/"outside"/"sce_sys"/"param.sfo");
    fs::create_directory_symlink(base/"outside",base/"titles"/"linked");
    options={};
    assert(pkgmaster::scan_package_library({(base/"titles").string()},options,results,error));
    for(const auto& item:results) assert(item.package_path.find("linked")==std::string::npos);
#endif
    fs::remove_all(base);
    std::cout<<"package library scanner tests passed\n";
}
