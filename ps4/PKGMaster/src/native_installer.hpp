#pragma once

#include <cstdint>
#include <string>

namespace pkgmaster {

// Thin owner-thread adapter around the PS4 AppInstUtil + BGFT package pipeline.
// The adapter only submits an existing local PKG path; it never downloads or
// modifies package contents.
class NativePackageInstaller {
public:
    NativePackageInstaller();
    ~NativePackageInstaller();

    NativePackageInstaller(const NativePackageInstaller&) = delete;
    NativePackageInstaller& operator=(const NativePackageInstaller&) = delete;

    bool initialize(std::string& error, int32_t& nativeError);
    bool enqueueLocalPackage(const std::string& path,
                             const std::string& displayName,
                             const std::string& expectedTitleId,
                             uint64_t packageSize,
                             int32_t& taskId,
                             std::string& error,
                             int32_t& nativeError);
    bool readProgress(int32_t taskId, uint64_t& transferred,
                      uint64_t& total, int32_t& preparingPercent,
                      int32_t& localCopyPercent, int32_t& taskError,
                      std::string& error, int32_t& nativeError);
    void shutdown();

private:
    bool loadRequiredModules(std::string& error, int32_t& nativeError);
    void* bgftHeap_;
    bool modulesLoaded_;
    bool appInstInitialized_;
    bool bgftInitialized_;
    int32_t userId_;
};

} // namespace pkgmaster
