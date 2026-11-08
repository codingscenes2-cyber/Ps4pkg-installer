#include "pkgmaster/platform/secure_random.hpp"
#include <cassert>
#include <string>
static bool is_hex_token(const std::string& token) {
    if (token.size() != 64) return false;
    for (char c : token)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
int main() {
    std::string first = "unchanged", second, error;
    assert(pkgmaster::generate_secure_token_hex(first, error));
    assert(error.empty() && is_hex_token(first));
    assert(pkgmaster::generate_secure_token_hex(second, error));
    assert(is_hex_token(second));
    assert(first != second);
}
