#include "pkgmaster/package/library_index_store.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#endif
namespace fs = std::filesystem;
static pkgmaster::LibraryPackage make_item(const std::string& id, const std::string& path) {
    pkgmaster::LibraryPackage item;
    item.metadata.title = "Title\twith newline\n";
    item.metadata.title_id = id;
    item.metadata.app_version = "01.00";
    item.metadata.category = "gd";
    item.metadata.system_version = "07000000";
    item.package_path = path;
    item.param_sfo_path = path + "/sce_sys/param.sfo";
    item.param_sfo_bytes = 1234;
    return item;
}
int main() {
    const auto base = fs::temp_directory_path() / ("pkgmaster-index-" + std::to_string(
#ifndef _WIN32
        static_cast<unsigned long long>(::getpid())
#else
        987655ULL
#endif
    ));
    fs::remove_all(base);
    fs::create_directories(base);
    const auto file = base / "library.idx";
    std::vector<pkgmaster::LibraryPackage> input{
        make_item("CUSA99999", "/titles/z"), make_item("CUSA11111", "/titles/a")};
    std::string error;
    assert(pkgmaster::PackageLibraryIndexStore::save(file.string(), input, error));
    std::vector<pkgmaster::LibraryPackage> loaded;
    assert(pkgmaster::PackageLibraryIndexStore::load(file.string(), loaded, error));
    assert(loaded.size() == 2);
    assert(loaded[0].metadata.title_id == "CUSA11111");
    assert(loaded[0].metadata.title == input[0].metadata.title);
    assert(loaded[1].param_sfo_bytes == 1234);
    auto unchanged = loaded;
    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << "PKGMASTER_LIBRARY\t999\n";
    }
    assert(!pkgmaster::PackageLibraryIndexStore::load(file.string(), loaded, error));
    assert(loaded.size() == unchanged.size());
    assert(loaded[0].metadata.title_id == unchanged[0].metadata.title_id);
    auto duplicate = input;
    duplicate[1].param_sfo_path = duplicate[0].param_sfo_path;
    assert(!pkgmaster::PackageLibraryIndexStore::save(file.string(), duplicate, error));
    assert(!pkgmaster::PackageLibraryIndexStore::save("", input, error));
    fs::remove_all(base);
    std::cout << "package library index tests passed\n";
}
