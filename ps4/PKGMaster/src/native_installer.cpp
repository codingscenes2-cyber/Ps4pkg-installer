#include "native_installer.hpp"

#include <orbis/AppInstUtil.h>
#include <orbis/Bgft.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

// OpenOrbis v0.5.4's public BGFT declarations use 32-bit package-size and
// progress fields. The PS4 service ABI uses 64-bit fields for these values.
// Keep the native layout private and guarded by compile-time ABI assertions.
struct NativeBgftDownloadParam {
    int32_t userId;
    int32_t entitlementType;
    const char* id;
    const char* contentUrl;
    const char* contentExUrl;
    const char* contentName;
    const char* iconPath;
    const char* skuId;
    int32_t option;
    const char* playgoScenarioId;
    const char* releaseDate;
    const char* packageType;
    const char* packageSubType;
    uint64_t packageSize;
};

struct NativeBgftDownloadParamEx {
    NativeBgftDownloadParam params;
    uint32_t slot;
};

struct NativeBgftTaskProgress {
    uint32_t bits;
    int32_t errorResult;
    uint64_t length;
    uint64_t transferred;
    uint64_t lengthTotal;
    uint64_t transferredTotal;
    uint32_t numIndex;
    uint32_t numTotal;
    uint32_t restSec;
    uint32_t restSecTotal;
    int32_t preparingPercent;
    int32_t localCopyPercent;
};

static_assert(offsetof(NativeBgftDownloadParam, packageSize) == 96,
              "Unexpected native BGFT package-size offset");
static_assert(sizeof(NativeBgftDownloadParam) == 104,
              "Unexpected native BGFT download parameter size");
static_assert(sizeof(NativeBgftDownloadParamEx) == 112,
              "Unexpected native BGFT extended parameter size");
static_assert(sizeof(NativeBgftTaskProgress) == 64,
              "Unexpected native BGFT progress structure size");

constexpr size_t kBgftHeapBytes = 1024u * 1024u;

struct RequiredModule {
    const char* filename;
    const char* label;
};

const RequiredModule kRequiredModules[] = {
    {"libSceSystemService.sprx", "System Service"},
    {"libSceAppInstUtil.sprx", "AppInstUtil"},
    {"libSceSysUtil.sprx", "SysUtil"},
    {"libSceBgft.sprx", "BGFT"}
};

std::string nativeErrorText(const char* operation, int32_t result) {
    std::ostringstream stream;
    stream << operation << " failed (0x";
    stream.setf(std::ios::hex, std::ios::basefield);
    stream.width(8);
    stream.fill('0');
    stream << static_cast<uint32_t>(result) << ")";
    return stream.str();
}

int32_t loadModuleByPath(const char* filename) {
    char path[256] = {};
    std::snprintf(path, sizeof(path), "/system/common/lib/%s", filename);
    int32_t result = static_cast<int32_t>(
        sceKernelLoadStartModule(path, 0, nullptr, 0, nullptr, nullptr));
    if (result >= 0) return result;

    const char* sandbox = sceKernelGetFsSandboxRandomWord();
    if (sandbox && sandbox[0]) {
        std::snprintf(path, sizeof(path), "/%s/common/lib/%s", sandbox, filename);
        result = static_cast<int32_t>(
            sceKernelLoadStartModule(path, 0, nullptr, 0, nullptr, nullptr));
    }
    return result;
}

bool inspectPkgFile(const std::string& path, uint64_t expectedSize) {
    const int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;

    struct stat info {};
    const bool regularAndSameSize = fstat(fd, &info) == 0 &&
        S_ISREG(info.st_mode) && info.st_size > 0 &&
        static_cast<uint64_t>(info.st_size) == expectedSize;

    std::array<unsigned char, 4> magic{};
    const ssize_t count = regularAndSameSize
        ? read(fd, magic.data(), magic.size()) : -1;
    close(fd);

    return count == static_cast<ssize_t>(magic.size()) &&
        magic[0] == 0x7f && magic[1] == 'C' &&
        magic[2] == 'N' && magic[3] == 'T';
}

} // namespace

extern "C" {
int32_t pkgmasterBgftRegisterStorage(NativeBgftDownloadParamEx* params,
                                     int32_t* taskId)
    __asm__("sceBgftServiceIntDownloadRegisterTaskByStorageEx");
int32_t pkgmasterBgftGetProgress(int32_t taskId,
                                 NativeBgftTaskProgress* progress)
    __asm__("sceBgftServiceDownloadGetProgress");
}

namespace pkgmaster {

NativePackageInstaller::NativePackageInstaller()
    : bgftHeap_(nullptr), modulesLoaded_(false),
      appInstInitialized_(false), bgftInitialized_(false), userId_(-1) {}

NativePackageInstaller::~NativePackageInstaller() {
    shutdown();
}

bool NativePackageInstaller::loadRequiredModules(std::string& error,
                                                 int32_t& nativeError) {
    if (modulesLoaded_) return true;
    for (const auto& module : kRequiredModules) {
        const int32_t result = loadModuleByPath(module.filename);
        if (result < 0) {
            nativeError = result;
            error = std::string("Could not load ") + module.label +
                    " system module (native error 0x";
            std::ostringstream code;
            code.setf(std::ios::hex, std::ios::basefield);
            code << static_cast<uint32_t>(result) << ")";
            error += code.str();
            return false;
        }
    }
    modulesLoaded_ = true;
    return true;
}

bool NativePackageInstaller::initialize(std::string& error,
                                        int32_t& nativeError) {
    nativeError = 0;
    if (bgftInitialized_ && appInstInitialized_) return true;

    if (!loadRequiredModules(error, nativeError)) return false;

    if (!appInstInitialized_) {
        const int32_t result = sceAppInstUtilInitialize();
        if (result != 0) {
            nativeError = result;
            error = nativeErrorText("sceAppInstUtilInitialize", result);
            return false;
        }
        appInstInitialized_ = true;
    }

    if (sceUserServiceGetInitialUser(&userId_) < 0 || userId_ < 0) {
        error = "No signed-in local PS4 user is available.";
        return false;
    }

    if (!bgftHeap_) {
        bgftHeap_ = std::malloc(kBgftHeapBytes);
        if (!bgftHeap_) {
            error = "Could not allocate BGFT service memory.";
            return false;
        }
        std::memset(bgftHeap_, 0, kBgftHeapBytes);
    }

    OrbisBgftInitParams params{};
    params.heap = bgftHeap_;
    params.heapSize = kBgftHeapBytes;
    const int32_t result = sceBgftServiceIntInit(&params);
    if (result != 0) {
        nativeError = result;
        error = nativeErrorText("sceBgftServiceIntInit", result);
        return false;
    }

    bgftInitialized_ = true;
    error.clear();
    return true;
}

bool NativePackageInstaller::enqueueLocalPackage(
    const std::string& path, const std::string& displayName,
    const std::string& expectedTitleId, uint64_t packageSize,
    int32_t& taskId, std::string& error, int32_t& nativeError) {
    taskId = -1;
    nativeError = 0;

    if (path.empty() || packageSize < 0x20) {
        error = "Package path, title ID or file size is invalid.";
        return false;
    }

    if (!inspectPkgFile(path, packageSize)) {
        error = "Package file is missing, changed, non-regular or has an invalid PS4 PKG header.";
        return false;
    }

    if (!initialize(error, nativeError)) return false;

    char titleId[16] = {};
    int32_t isApplication = 0;
    int32_t result = sceAppInstUtilGetTitleIdFromPkg(
        path.c_str(), titleId, &isApplication);
    if (result != 0) {
        nativeError = result;
        error = nativeErrorText("sceAppInstUtilGetTitleIdFromPkg", result);
        return false;
    }

    if (titleId[0] == 0) {
        error = "The PS4 package service returned an empty title ID.";
        return false;
    }
    if (!expectedTitleId.empty() && expectedTitleId != titleId) {
        error = "PKG title ID changed or does not match the inspected metadata.";
        return false;
    }

    NativeBgftDownloadParamEx params{};
    params.params.userId = userId_;
    params.params.entitlementType = 5;
    params.params.id = "";
    params.params.contentUrl = path.c_str();
    params.params.contentExUrl = "";
    params.params.contentName = displayName.c_str();
    params.params.iconPath = "/app0/sce_sys/icon0.png";
    params.params.skuId = "";
    params.params.option = ORBIS_BGFT_TASK_OPT_NONE;
    params.params.playgoScenarioId = "0";
    params.params.releaseDate = "";
    params.params.packageType = "";
    params.params.packageSubType = "";
    params.params.packageSize = packageSize;
    params.slot = 0;

    result = pkgmasterBgftRegisterStorage(&params, &taskId);
    if (result != 0) {
        taskId = -1;
        nativeError = result;
        error = nativeErrorText(
            "sceBgftServiceIntDownloadRegisterTaskByStorageEx", result);
        return false;
    }

    result = sceBgftServiceDownloadStartTask(taskId);
    if (result != 0) {
        sceBgftServiceIntDownloadUnregisterTask(taskId);
        taskId = -1;
        nativeError = result;
        error = nativeErrorText("sceBgftServiceDownloadStartTask", result);
        return false;
    }

    error.clear();
    return true;
}

bool NativePackageInstaller::readProgress(
    int32_t taskId, uint64_t& transferred, uint64_t& total,
    int32_t& preparingPercent, int32_t& localCopyPercent,
    int32_t& taskError, std::string& error, int32_t& nativeError) {
    NativeBgftTaskProgress progress{};
    nativeError = 0;
    transferred = 0;
    total = 0;
    preparingPercent = 0;
    localCopyPercent = 0;
    taskError = 0;

    const int32_t result = pkgmasterBgftGetProgress(taskId, &progress);
    if (result != 0) {
        nativeError = result;
        error = nativeErrorText("sceBgftServiceDownloadGetProgress", result);
        return false;
    }

    transferred = progress.transferredTotal ? progress.transferredTotal
                                             : progress.transferred;
    total = progress.lengthTotal ? progress.lengthTotal : progress.length;
    preparingPercent = progress.preparingPercent;
    localCopyPercent = progress.localCopyPercent;
    taskError = progress.errorResult;
    error.clear();
    return true;
}

void NativePackageInstaller::shutdown() {
    if (bgftInitialized_) {
        sceBgftServiceIntTerm();
        bgftInitialized_ = false;
    }
    if (appInstInitialized_) {
        sceAppInstUtilTerminate();
        appInstInitialized_ = false;
    }
    if (bgftHeap_) {
        std::free(bgftHeap_);
        bgftHeap_ = nullptr;
    }
}

} // namespace pkgmaster
