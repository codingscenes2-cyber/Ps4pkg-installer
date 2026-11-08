#include "pkg_scanner.hpp"

#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <sys/stat.h>

namespace {
constexpr const char* kUsbRoots[] = {
    "/mnt/usb0",
    "/mnt/usb1",
    "/mnt/usb2",
    "/mnt/usb3"
};
constexpr size_t kMaxPackages = 64;

bool hasPkgExtension(const std::string& name) {
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos) return false;

    std::string extension = name.substr(dot);
    for (char& ch : extension) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return extension == ".pkg";
}
}

std::vector<PackageEntry> scanUsbPackages() {
    std::vector<PackageEntry> packages;

    for (const char* root : kUsbRoots) {
        DIR* directory = opendir(root);
        if (!directory) continue;

        dirent* entry = nullptr;
        while ((entry = readdir(directory)) != nullptr) {
            if (packages.size() >= kMaxPackages) break;

            const std::string name(entry->d_name);
            if (name.empty() || name == "." || name == ".." || !hasPkgExtension(name)) {
                continue;
            }

            const std::string path = std::string(root) + "/" + name;
            struct stat info {};
            if (stat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode) || info.st_size <= 0) {
                continue;
            }

            packages.push_back(PackageEntry{
                name,
                path,
                static_cast<uint64_t>(info.st_size)
            });
        }

        closedir(directory);
        if (packages.size() >= kMaxPackages) break;
    }

    std::sort(packages.begin(), packages.end(),
              [](const PackageEntry& left, const PackageEntry& right) {
                  return left.name < right.name;
              });
    return packages;
}
