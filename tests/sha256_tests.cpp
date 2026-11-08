#include "pkgmaster/core/sha256.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
int main() {
    const std::string abc="abc";
    assert(pkgmaster::sha256_hex(abc.data(),abc.size())==
           "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    assert(pkgmaster::is_sha256_hex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    assert(!pkgmaster::is_sha256_hex("not-a-digest"));
    const std::string path="pkgmaster-sha256-test.bin";
    {std::ofstream out(path,std::ios::binary);out<<abc;}
    std::string digest,error;
    assert(pkgmaster::sha256_file(path,digest,error));
    assert(digest=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    std::remove(path.c_str());
    return 0;
}
