#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct PackageEntry {
    std::string name;
    std::string path;
    uint64_t sizeBytes;
};

// Scans the root of the four standard PS4 USB mount points for regular .pkg files.
std::vector<PackageEntry> scanUsbPackages();
