#pragma once
#include <string>

namespace pkgmaster {
// Generates a 256-bit lowercase hexadecimal token from the OS CSPRNG.
// The output is unchanged on failure.
bool generate_secure_token_hex(std::string& output, std::string& error);
}
