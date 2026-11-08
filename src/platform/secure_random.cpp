#include "pkgmaster/platform/secure_random.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#elif defined(__unix__) || defined(__APPLE__) || defined(__ORBIS__)
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace pkgmaster {
namespace {
void wipe(std::array<std::uint8_t, 32>& bytes) noexcept {
    volatile std::uint8_t* p = bytes.data();
    for (std::size_t i = 0; i < bytes.size(); ++i) p[i] = 0;
}
}
bool generate_secure_token_hex(std::string& output, std::string& error) {
    error.clear();
    std::array<std::uint8_t, 32> bytes{};
#if defined(_WIN32)
    const NTSTATUS status = BCryptGenRandom(nullptr, bytes.data(),
        static_cast<ULONG>(bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status < 0) {
        error = "Operating-system random generator failed";
        wipe(bytes);
        return false;
    }
#elif defined(__unix__) || defined(__APPLE__) || defined(__ORBIS__)
    const int fd = ::open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        error = "Could not open operating-system random source";
        wipe(bytes);
        return false;
    }
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto count = ::read(fd, bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            error = "Could not read enough bytes from operating-system random source";
            ::close(fd);
            wipe(bytes);
            return false;
        }
        offset += static_cast<std::size_t>(count);
    }
    ::close(fd);
#else
    error = "No supported operating-system random source for this platform";
    wipe(bytes);
    return false;
#endif
    static constexpr char digits[] = "0123456789abcdef";
    std::string token;
    token.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        token.push_back(digits[byte >> 4]);
        token.push_back(digits[byte & 0x0f]);
    }
    wipe(bytes);
    output = std::move(token);
    return true;
}
}
