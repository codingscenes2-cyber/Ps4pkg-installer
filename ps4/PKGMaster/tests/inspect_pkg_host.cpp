#include "pkg_inspector.hpp"

#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: inspect_pkg_host <package.pkg>\n";
        return 2;
    }

    pkgmaster::PackageMetadata metadata;
    std::string error;
    if (!inspect_pkg_param_sfo(argv[1], metadata, error)) {
        std::cerr << "PKG metadata inspection failed: " << error << "\n";
        return 1;
    }

    std::cout << "TITLE=" << metadata.title << "\n";
    std::cout << "TITLE_ID=" << metadata.title_id << "\n";
    std::cout << "APP_VER=" << metadata.app_version << "\n";
    std::cout << "CATEGORY=" << metadata.category << "\n";

    if (metadata.title.empty() || metadata.title_id.empty()) {
        std::cerr << "PKG metadata is missing TITLE or TITLE_ID.\n";
        return 1;
    }
    return 0;
}
