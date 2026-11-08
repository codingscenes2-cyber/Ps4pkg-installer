#include "pkgmaster/platform/phone_http_parser.hpp"
#include <algorithm>
#include <cctype>
#include <limits>

namespace pkgmaster {
namespace {
bool token_char(unsigned char c) {
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z')) return true;
    switch (c) {
        case '!': case '#': case '$': case '%': case '&': case '\'': case '*':
        case '+': case '-': case '.': case '^': case '_': case '`': case '|': case '~':
            return true;
        default: return false;
    }
}
bool valid_limits(const PhoneHttpParseLimits& l) {
    return l.max_request_bytes >= 4 && l.max_request_bytes <= 1024 * 1024 &&
           l.max_header_bytes >= 16 && l.max_header_bytes <= 64 * 1024 &&
           l.max_body_bytes <= 512 * 1024 && l.max_headers > 0 && l.max_headers <= 128 &&
           l.max_header_bytes <= l.max_request_bytes;
}
bool parse_length(const std::string& value, std::size_t& output) {
    if (value.empty()) return false;
    std::size_t parsed = 0;
    for (char c : value) {
        if (c < '0' || c > '9') return false;
        const auto digit = static_cast<std::size_t>(c - '0');
        if (parsed > (std::numeric_limits<std::size_t>::max() - digit) / 10) return false;
        parsed = parsed * 10 + digit;
    }
    output = parsed;
    return true;
}
bool has_bad_control(const std::string& value, bool allow_tab) {
    for (unsigned char c : value)
        if ((c < 0x20 && !(allow_tab && c == '\t')) || c == 0x7f) return true;
    return false;
}
std::string trim_ows(const std::string& value) {
    std::size_t first = 0, last = value.size();
    while (first < last && (value[first] == ' ' || value[first] == '\t')) ++first;
    while (last > first && (value[last - 1] == ' ' || value[last - 1] == '\t')) --last;
    return value.substr(first, last - first);
}
}
bool parse_phone_http_request(const std::string& bytes,
                              const PhoneHttpParseLimits& limits,
                              PhoneHttpRequest& output,
                              std::string& error) {
    error.clear();
    if (!valid_limits(limits)) { error = "Invalid HTTP parser limits"; return false; }
    if (bytes.empty() || bytes.size() > limits.max_request_bytes) {
        error = "HTTP request is empty or exceeds the request limit"; return false;
    }
    const auto separator = bytes.find("\r\n\r\n");
    if (separator == std::string::npos || separator + 4 > limits.max_header_bytes) {
        error = "HTTP header is incomplete or exceeds the header limit"; return false;
    }
    const std::string header_block = bytes.substr(0, separator);
    if (header_block.find('\0') != std::string::npos || header_block.find('\n') == std::string::npos) {
        error = "HTTP header contains invalid bytes"; return false;
    }
    PhoneHttpRequest parsed;
    const auto first_end = header_block.find("\r\n");
    const std::string request_line = header_block.substr(0, first_end);
    const auto first_space = request_line.find(' ');
    const auto second_space = first_space == std::string::npos ? first_space : request_line.find(' ', first_space + 1);
    if (first_space == std::string::npos || second_space == std::string::npos ||
        request_line.find(' ', second_space + 1) != std::string::npos) {
        error = "Malformed HTTP request line"; return false;
    }
    parsed.method = request_line.substr(0, first_space);
    parsed.target = request_line.substr(first_space + 1, second_space - first_space - 1);
    parsed.version = request_line.substr(second_space + 1);
    if ((parsed.method != "GET" && parsed.method != "POST") ||
        parsed.version != "HTTP/1.1" || parsed.target.empty() || parsed.target[0] != '/' ||
        parsed.target.find('#') != std::string::npos || has_bad_control(parsed.target, false) ||
        parsed.target.find('\\') != std::string::npos) {
        error = "Unsupported method, request target, or HTTP version"; return false;
    }
    std::size_t line_start = first_end == std::string::npos ? header_block.size() : first_end + 2;
    while (line_start < header_block.size()) {
        const auto line_end = header_block.find("\r\n", line_start);
        const std::string line = header_block.substr(line_start,
            line_end == std::string::npos ? std::string::npos : line_end - line_start);
        if (line.empty() || line[0] == ' ' || line[0] == '\t' || has_bad_control(line, true)) {
            error = "Malformed or folded HTTP header"; return false;
        }
        const auto colon = line.find(':');
        if (colon == std::string::npos || colon == 0) {
            error = "HTTP header is missing a valid field name"; return false;
        }
        std::string name = line.substr(0, colon);
        for (char& c : name) {
            const auto u = static_cast<unsigned char>(c);
            if (!token_char(u)) { error = "HTTP header name contains an invalid character"; return false; }
            c = static_cast<char>(std::tolower(u));
        }
        const std::string value = trim_ows(line.substr(colon + 1));
        if (parsed.headers.size() >= limits.max_headers ||
            !parsed.headers.emplace(name, value).second) {
            error = "Too many or duplicate HTTP headers"; return false;
        }
        if (line_end == std::string::npos) break;
        line_start = line_end + 2;
    }
    const auto host = parsed.headers.find("host");
    if (host == parsed.headers.end() || host->second.empty()) {
        error = "HTTP/1.1 Host header is required"; return false;
    }
    if (parsed.headers.find("transfer-encoding") != parsed.headers.end()) {
        error = "Transfer-Encoding is not supported"; return false;
    }
    const auto length_it = parsed.headers.find("content-length");
    std::size_t body_length = 0;
    if (length_it != parsed.headers.end() && !parse_length(length_it->second, body_length)) {
        error = "Invalid Content-Length"; return false;
    }
    if (body_length > limits.max_body_bytes ||
        (parsed.method == "GET" && body_length != 0) ||
        (parsed.method == "POST" && length_it == parsed.headers.end())) {
        error = "HTTP body length is not permitted"; return false;
    }
    if (separator + 4 > bytes.size() || body_length != bytes.size() - separator - 4) {
        error = "HTTP body is incomplete or contains trailing/pipelined data"; return false;
    }
    parsed.body.assign(bytes.data() + separator + 4, body_length);
    output = std::move(parsed);
    return true;
}
}
