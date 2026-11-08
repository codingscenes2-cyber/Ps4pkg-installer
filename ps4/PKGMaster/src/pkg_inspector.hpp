#pragma once

#include "pkgmaster/package/param_sfo.hpp"
#include <string>

// Reads the unencrypted PARAM.SFO metadata entry from a PS4 PKG container.
bool inspect_pkg_param_sfo(const std::string& pkgPath,
                           pkgmaster::PackageMetadata& metadata,
                           std::string& error);
