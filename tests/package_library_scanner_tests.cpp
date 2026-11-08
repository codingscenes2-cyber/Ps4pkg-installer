#include "pkgmaster/package/library_scanner.hpp"
#include <cassert>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
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
    const std::string key("TITLE\0TITLE_ID\0",15);
    const std::string title("Scanner Test\0",13), id("CUSA12345\0",10);
    const std::size_t key_off=20+2*16, data_off=key_off+key.size();
    std::vector<std::uint8_t> b(data_off+title.size()+id.size());
    put32(b,0,0x46535000); put32(b,4,0x00000101); put32(b,8,static_cast<std::uint32_t>(key_off));
    put32(b,12,static_cast<std::uint32_t>(data_off)); put32(b,16,2); put16(b,20,0); put16(b,22,0x0204); put32(b,24,title.size()); put32(b,28,title.size()); put32(b,32,0);
    put16(b,36,6); put16(b,38,0x0204); put32(b,40,id.size()); put32(b,44,id.size()); put32(b,48,title.size());
    std::copy(key.begin(),key.end(),b.begin()+key_off);
    std::copy(title.begin(),title.end(),b.begin()+data_off);
    std::copy(id.begin(),id.end(),b.begin()+data_off+title.size());
    return b;
}
static void write_sfo(const fs::path& p, bool valid=true) {
    fs::create_directories(p.parent_path());
    auto bytes=make_sfo(); if(!valid) bytes[0]=0;
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
    write_sfo(base/"titles"/"deep"/"a"/"b"/"sce_sys"/"param.sfo");
    std::vector<pkgmaster::LibraryPackage> results;
    std::string error;
    pkgmaster::PackageScanOptions options;
    assert(pkgmaster::scan_package_library({(base/"titles").string()},options,results,error));
    assert(results.size()==3);
    assert(results[0].metadata.title=="Scanner Test");
    assert(results[0].metadata.title_id=="CUSA12345");
    assert(results[0].param_sfo_bytes>0);
    options.max_depth=0;
    assert(pkgmaster::scan_package_library({(base/"titles").string()},options,results,error));
    assert(results.empty());
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
