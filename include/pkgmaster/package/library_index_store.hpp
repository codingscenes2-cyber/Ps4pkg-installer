#pragma once
#include "pkgmaster/package/library_scanner.hpp"
#include <string>
#include <vector>

namespace pkgmaster {
class PackageLibraryIndexStore {
public:
    // Stores metadata and extracted-directory paths only; no package payload bytes.
    static bool save(const std::string& path,
                     const std::vector<LibraryPackage>& packages,
                     std::string& error);
    // Transactional load: packages is unchanged if validation or I/O fails.
    static bool load(const std::string& path,
                     std::vector<LibraryPackage>& packages,
                     std::string& error);
};
}
