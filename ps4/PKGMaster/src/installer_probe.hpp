#pragma once

#include <cstdint>
#include <string>

struct InstallerProbeResult {
    int32_t bgftModuleResult{};
    int32_t bgftInitResult{};
    int32_t appInstModuleHandle{};
    int32_t getTitleIdSymbolResult{};
    int32_t getTitleIdResult{};
    int32_t isApp{-1};
    char titleId[18]{};
};

void probeInstallerServices(const std::string& pkgPath,
                            InstallerProbeResult& result);
