#include "pkgmaster/downloader/http_downloader.hpp"
#include "pkgmaster/core/url_policy.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#ifdef PKGMASTER_HAS_CURL
#include <curl/curl.h>
#if defined(__unix__) || defined(__APPLE__)
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#endif

namespace pkgmaster {
#ifdef PKGMASTER_HAS_CURL
namespace {
struct TransferContext {
    std::FILE* file{};
    const HttpDownloadOptions* options{};
    HttpDownloadResult* result{};
    bool write_failed{};
    bool too_large{};
};
#if defined(__unix__) || defined(__APPLE__)
bool is_public_address(const sockaddr* address) {
    if (!address) return false;
    if (address->sa_family == AF_INET) {
        const auto* in = reinterpret_cast<const sockaddr_in*>(address);
        const std::uint32_t ip = ntohl(in->sin_addr.s_addr);
        const unsigned a = (ip >> 24) & 255u, b = (ip >> 16) & 255u;
        if (a == 0 || a == 10 || a == 127 || a >= 224) return false;
        if ((a == 169 && b == 254) || (a == 172 && b >= 16 && b <= 31) ||
            (a == 192 && b == 168) || (a == 100 && b >= 64 && b <= 127) ||
            (a == 192 && b == 0) || (a == 198 && (b == 18 || b == 19))) return false;
        return true;
    }
    if (address->sa_family == AF_INET6) {
        const auto* in6 = reinterpret_cast<const sockaddr_in6*>(address);
        const auto* bytes = in6->sin6_addr.s6_addr;
        if (IN6_IS_ADDR_UNSPECIFIED(&in6->sin6_addr) ||
            IN6_IS_ADDR_LOOPBACK(&in6->sin6_addr) ||
            IN6_IS_ADDR_LINKLOCAL(&in6->sin6_addr) ||
            IN6_IS_ADDR_MULTICAST(&in6->sin6_addr) ||
            IN6_IS_ADDR_SITELOCAL(&in6->sin6_addr) ||
            (bytes[0] & 0xfeu) == 0xfcu) return false;
        if (IN6_IS_ADDR_V4MAPPED(&in6->sin6_addr)) {
            sockaddr_in mapped{};
            mapped.sin_family = AF_INET;
            std::memcpy(&mapped.sin_addr, bytes + 12, 4);
            return is_public_address(reinterpret_cast<const sockaddr*>(&mapped));
        }
        return true;
    }
    return false;
}
curl_socket_t guarded_open_socket(void*, curlsocktype purpose,
                                  struct curl_sockaddr* address) {
    if (purpose != CURLSOCKTYPE_IPCXN ||
        !is_public_address(reinterpret_cast<const sockaddr*>(&address->addr)))
        return CURL_SOCKET_BAD;
    return ::socket(address->family, address->socktype, address->protocol);
}
#endif

size_t write_body(char* data, size_t size, size_t count, void* opaque) {
    auto* ctx = static_cast<TransferContext*>(opaque);
    if (size && count > std::numeric_limits<std::size_t>::max() / size) return 0;
    const std::size_t bytes = size * count;
    if (ctx->result->received_bytes > ctx->options->maximum_bytes ||
        bytes > ctx->options->maximum_bytes - ctx->result->received_bytes) {
        ctx->too_large = true;
        return 0;
    }
    const auto written = std::fwrite(data, 1, bytes, ctx->file);
    ctx->result->received_bytes += written;
    if (written != bytes) ctx->write_failed = true;
    return written;
}
int report_progress(void* opaque, curl_off_t total, curl_off_t now,
                    curl_off_t, curl_off_t) {
    auto* ctx = static_cast<TransferContext*>(opaque);
    if (ctx->options->cancel && ctx->options->cancel->load()) return 1;
    if (ctx->options->progress || ctx->options->metrics) {
        const auto received = now > 0 ? static_cast<std::uint64_t>(now) : 0;
        const auto expected = total > 0 ? static_cast<std::uint64_t>(total) : 0;
        try {
            if (ctx->options->progress) ctx->options->progress(received, expected);
            if (ctx->options->metrics)
                ctx->options->metrics(ctx->speed_tracker.update(received, expected));
        } catch (...) { return 1; }
    }
    return 0;
}
}
#endif

HttpDownloadResult download_http_file(const HttpDownloadOptions& options) {
    HttpDownloadResult result;
    if (options.url.empty() || options.destination_path.empty()) {
        result.error = "A source URL and destination path are required.";
        return result;
    }
    std::string policy_error;
    if (!is_allowed_download_url(options.url, policy_error)) {
        result.error = policy_error;
        return result;
    }
    if (!options.maximum_bytes || options.connect_timeout_ms <= 0 ||
        options.total_timeout_seconds < 0 ||\n        options.maximum_bytes_per_second > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        result.error = "Invalid download limits or timeout settings.";
        return result;
    }
#ifndef PKGMASTER_HAS_CURL
    result.status = DownloadStatus::Unsupported;
    result.error = "This build has no libcurl HTTP transport.";
    return result;
#else
    if (options.cancel && options.cancel->load()) {
        result.status = DownloadStatus::Cancelled;
        result.error = "Download cancelled before starting.";
        return result;
    }
    std::ifstream existing(options.destination_path, std::ios::binary);
    if (existing.good() && !options.overwrite) {
        result.error = "Destination already exists; overwrite was not enabled.";
        return result;
    }
    const std::string temporary = options.destination_path + ".part";
    std::FILE* file = std::fopen(temporary.c_str(), "wb");
    if (!file) {
        result.error = "Could not create temporary download file.";
        return result;
    }
    static const CURLcode global_init = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (global_init != CURLE_OK) {
        std::fclose(file);
        std::remove(temporary.c_str());
        result.error = "libcurl global initialization failed.";
        return result;
    }
    CURL* raw = curl_easy_init();
    if (!raw) {
        std::fclose(file);
        std::remove(temporary.c_str());
        result.error = "libcurl could not initialize.";
        return result;
    }
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(raw, &curl_easy_cleanup);
    TransferContext context{file, &options, &result, false, false};
    curl_easy_setopt(curl.get(), CURLOPT_URL, options.url.c_str());
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_PROXY, "");
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT_MS, options.connect_timeout_ms);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, options.total_timeout_seconds);\n    if (options.maximum_bytes_per_second)\n        curl_easy_setopt(curl.get(), CURLOPT_MAX_RECV_SPEED_LARGE, static_cast<curl_off_t>(options.maximum_bytes_per_second));
    curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "PKGMaster/0.1");
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &context);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, report_progress);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &context);
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
#if defined(__unix__) || defined(__APPLE__)
    curl_easy_setopt(curl.get(), CURLOPT_OPENSOCKETFUNCTION, guarded_open_socket);
#endif
    const CURLcode code = curl_easy_perform(curl.get());
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &result.http_status);
    curl_off_t remote_size = -1;
    curl_easy_getinfo(curl.get(), CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &remote_size);
    if (remote_size >= 0) result.expected_bytes = static_cast<std::uint64_t>(remote_size);
    const bool flush_ok = std::fflush(file) == 0;
    const bool close_ok = std::fclose(file) == 0 && flush_ok;
    if (options.cancel && options.cancel->load()) {
        result.status = DownloadStatus::Cancelled;
        result.error = "Download cancelled.";
    } else if (context.too_large) {
        result.error = "Download exceeded the configured maximum size.";
    } else if (context.write_failed || !close_ok) {
        result.error = "Could not write the downloaded file.";
    } else if (code != CURLE_OK) {
        result.error = curl_easy_strerror(code);
    } else if (result.http_status >= 300 && result.http_status < 400) {
        result.error = "Redirect refused. Validate the redirect destination before following it.";
    } else if (result.http_status < 200 || result.http_status >= 300) {
        result.error = "HTTP server returned status " + std::to_string(result.http_status) + ".";
    } else if (result.expected_bytes && result.expected_bytes != result.received_bytes) {
        result.error = "Downloaded byte count does not match Content-Length.";
    } else if (result.received_bytes == 0) {
        result.error = "Server returned an empty response body.";
    } else if (std::rename(temporary.c_str(), options.destination_path.c_str()) == 0) {
        result.status = DownloadStatus::Completed;
        return result;
    } else {
        result.error = "Could not move completed download into its destination.";
    }
    std::remove(temporary.c_str());
    if (result.status != DownloadStatus::Cancelled)
        result.status = DownloadStatus::Failed;
    return result;
#endif
}
}
