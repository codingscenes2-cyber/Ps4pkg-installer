#include "installer_probe.hpp"

#include <cstdlib>
#include <cstring>

#include <orbis/Bgft.h>
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h>

namespace {
constexpr size_t kBgftHeapSize = 1024u * 1024u;
static void* g_bgftHeap = nullptr;
static bool g_bgftInitialized = false;
static int g_appInstModuleHandle = 0;

using GetTitleIdFromPkgFn = int32_t (*)(const char*, char*, int32_t*);
static GetTitleIdFromPkgFn g_getTitleIdFromPkg = nullptr;
}

void probeInstallerServices(const std::string& pkgPath,
                            InstallerProbeResult& result) {
    result = InstallerProbeResult{};

    if (!g_bgftInitialized) {
        result.bgftModuleResult = static_cast<int32_t>(
            sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_BGFT));

        if (result.bgftModuleResult == 0) {
            g_bgftHeap = std::malloc(kBgftHeapSize);
            if (!g_bgftHeap) {
                result.bgftInitResult = -1;
            } else {
                std::memset(g_bgftHeap, 0, kBgftHeapSize);
                OrbisBgftInitParams params{};
                params.heap = g_bgftHeap;
                params.heapSize = kBgftHeapSize;
                result.bgftInitResult = sceBgftServiceIntInit(&params);
                if (result.bgftInitResult == 0) {
                    g_bgftInitialized = true;
                } else {
                    std::free(g_bgftHeap);
                    g_bgftHeap = nullptr;
                }
            }
        } else {
            result.bgftInitResult = result.bgftModuleResult;
        }
    } else {
        result.bgftModuleResult = 0;
        result.bgftInitResult = 0;
    }

    if (g_appInstModuleHandle <= 0) {
        g_appInstModuleHandle = sceKernelLoadStartModule(
            "/system/common/lib/libSceAppInstUtil.sprx",
            0, nullptr, 0, nullptr, nullptr);
    }
    result.appInstModuleHandle = g_appInstModuleHandle;

    if (g_appInstModuleHandle > 0 && !g_getTitleIdFromPkg) {
        result.getTitleIdSymbolResult = sceKernelDlsym(
            g_appInstModuleHandle,
            "sceAppInstUtilGetTitleIdFromPkg",
            reinterpret_cast<void**>(&g_getTitleIdFromPkg));
    } else if (g_getTitleIdFromPkg) {
        result.getTitleIdSymbolResult = 0;
    } else {
        result.getTitleIdSymbolResult = g_appInstModuleHandle;
    }

    if (g_getTitleIdFromPkg && !pkgPath.empty()) {
        result.getTitleIdResult = g_getTitleIdFromPkg(
            pkgPath.c_str(), result.titleId, &result.isApp);
    } else {
        result.getTitleIdResult = result.getTitleIdSymbolResult != 0
            ? result.getTitleIdSymbolResult : -1;
    }
}
