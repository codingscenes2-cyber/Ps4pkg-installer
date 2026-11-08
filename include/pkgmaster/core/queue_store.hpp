#pragma once
#include "pkgmaster/core/job.hpp"
#include <string>
#include <vector>

namespace pkgmaster {
class QueueStore {
public:
    // Writes a versioned snapshot using a temporary file followed by rename.
    static bool save(const std::string& path, const std::vector<Job>& jobs,
                     std::string& error);
    // Loads and validates the complete snapshot. On failure, output is unchanged.
    static bool load(const std::string& path, std::vector<Job>& jobs,
                     std::string& error);
};
}
