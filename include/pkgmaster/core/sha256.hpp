#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace pkgmaster {
std::string sha256_hex(const void* data, std::size_t size);
bool is_sha256_hex(const std::string& value) noexcept;
bool sha256_file(const std::string& path, std::string& digest, std::string& error);
}
