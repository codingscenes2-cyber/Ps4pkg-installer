#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace pkgmaster {
struct ByteRange { std::uint64_t first{}, last{}; std::uint64_t length() const noexcept { return last-first+1; } };
struct RangePlan { std::uint64_t total_bytes{}; std::vector<ByteRange> parts; };
RangePlan make_range_plan(std::uint64_t total_bytes, unsigned workers, std::uint64_t minimum_part_bytes = 1024*1024);
struct RangeReply { int http_status{}; std::uint64_t first{}, last{}, total{}, body_bytes{}; std::string etag; };
bool validate_range_reply(const ByteRange&, const RangeReply&, std::uint64_t expected_total, std::string& reason);
}