#include "pkgmaster/platform/phone_http_parser.hpp"
#include <cassert>
#include <string>
int main() {
    pkgmaster::PhoneHttpRequest request;
    std::string error;
    const std::string get = "GET /api/v1/status HTTP/1.1\r\nHost: 192.168.1.20:8080\r\nAuthorization: Bearer abc\r\n\r\n";
    assert(pkgmaster::parse_phone_http_request(get, {}, request, error));
    assert(request.method == "GET" && request.target == "/api/v1/status");
    assert(request.headers.at("host") == "192.168.1.20:8080");
    assert(request.headers.at("authorization") == "Bearer abc");
    const std::string post = "POST /api/v1/pair HTTP/1.1\r\nHost: ps4.local\r\nContent-Length: 7\r\nContent-Type: application/json\r\n\r\n{\"x\":1}";
    assert(pkgmaster::parse_phone_http_request(post, {}, request, error));
    assert(request.body == "{\"x\":1}");
    auto unchanged = request;
    const std::string bad[] = {
        "GET / HTTP/1.1\r\n\r\n",
        "GET / HTTP/1.0\r\nHost: ps4\r\n\r\n",
        "PUT / HTTP/1.1\r\nHost: ps4\r\n\r\n",
        "GET / HTTP/1.1\r\nHost: ps4\r\nHost: other\r\n\r\n",
        "POST / HTTP/1.1\r\nHost: ps4\r\nTransfer-Encoding: chunked\r\nContent-Length: 0\r\n\r\n",
        "POST / HTTP/1.1\r\nHost: ps4\r\nContent-Length: 9\r\n\r\nshort",
        "GET / HTTP/1.1\r\nHost: ps4\r\n\r\nextra",
        "GET / HTTP/1.1\r\n Host: ps4\r\n\r\n",
        "GET http://evil/ HTTP/1.1\r\nHost: evil\r\n\r\n"
    };
    for (const auto& sample : bad) {
        assert(!pkgmaster::parse_phone_http_request(sample, {}, request, error));
        assert(request.method == unchanged.method && request.body == unchanged.body);
    }
    pkgmaster::PhoneHttpParseLimits limits;
    assert(pkgmaster::parse_phone_http_request("DELETE /api/v1/downloads/1 HTTP/1.1\r\nHost: ps4\r\n\r\n", {}, request, error));
    assert(request.method == "DELETE");
    limits.max_body_bytes = 2;
    assert(!pkgmaster::parse_phone_http_request(post, limits, request, error));
    limits = {};
    limits.max_headers = 1;
    assert(!pkgmaster::parse_phone_http_request(get, limits, request, error));
}
