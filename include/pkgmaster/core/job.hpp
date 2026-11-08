#pragma once
#include <cstdint>
#include <string>

namespace pkgmaster {
enum class JobState { Queued, Resolving, Downloading, Verifying, ReadyToInstall,
                       Installing, Paused, Completed, Failed, Cancelled };

struct Job {
    std::uint64_t id{};
    std::string source_url;
    std::string file_name;
    std::string local_path;
    std::uint64_t expected_bytes{};
    std::uint64_t received_bytes{};
    JobState state{JobState::Queued};
    std::string error;

    bool terminal() const noexcept;
    double progress() const noexcept;
};
}
