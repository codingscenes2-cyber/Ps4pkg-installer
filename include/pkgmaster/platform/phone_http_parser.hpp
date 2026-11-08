#pragma once
#include <cstddef>
#include <map>
#include <string>

namespace pkgmaster {
struct PhoneHttpRequest {
    std::string method;
    std::string target;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
};
struct PhoneHttpParseLimits {
    std::size_t max_request_bytes{128 * 1024};
    std::size_t max_header_bytes{16 * 1024};
    std::size_t max_body_bytes{64 * 1024};
    std::size_t max_headers{32};
};
// Parses one complete HTTP/1.1 request. Supports GET, POST and DELETE only, rejects
// transfer encoding and pipelined/trailing bytes. Output is unchanged on failure.
bool parse_phone_http_request(const std::string& bytes,
                              const PhoneHttpParseLimits& limits,
                              PhoneHttpRequest& output,
                              std::string& error);
}
