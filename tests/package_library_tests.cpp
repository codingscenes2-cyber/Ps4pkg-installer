#include "pkgmaster/core/package_library.hpp"
#include "pkgmaster/core/sha256.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
int main() {
    namespace fs=std::filesystem;
    const fs::path root="pkgmaster-library-test";
    fs::remove_all(root);
    fs::create_directories(root/"packages");
    const fs::path pkg=root/"packages"/"Example.PKG";
    {std::ofstream out(pkg,std::ios::binary);out<<"PKGDATA";}
    const fs::path index=root/"library.db";
    std::string error;
    pkgmaster::PackageLibrary library(index.string());
    std::uint64_t id=0;
    assert(library.add_package(pkg.string(),{},id,error));
    assert(id!=0);
    auto records=library.snapshot();
    assert(records.size()==1&&records[0].file_name=="Example.PKG");
    assert(records[0].size_bytes==7&&pkgmaster::is_sha256_hex(records[0].sha256));
    assert(library.search("example").size()==1);
    pkgmaster::PackageLibrary restored(index.string());
    assert(restored.load(error));
    assert(restored.snapshot().size()==1);
    assert(restored.remove(id,error));
    assert(restored.snapshot().empty());
    assert(fs::exists(pkg)); // Removing a catalog record never deletes the user's package.
    std::size_t added=0;
    assert(restored.scan_directory((root/"packages").string(),10,added,error));
    assert(added==1&&restored.snapshot().size()==1);
    assert(!restored.add_package(pkg.string(),"http://127.0.0.1/private",id,error));
    fs::remove_all(root);
    return 0;
}
