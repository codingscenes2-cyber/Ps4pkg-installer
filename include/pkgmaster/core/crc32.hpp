#pragma once
#include <cstddef>
#include <cstdint>

namespace pkgmaster {
inline std::uint32_t crc32_update(std::uint32_t previous,
                                  const void* data, std::size_t size) noexcept {
    auto crc = ~previous;
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
inline std::uint32_t crc32(const void* data, std::size_t size) noexcept {
    return crc32_update(0, data, size);
}
}
