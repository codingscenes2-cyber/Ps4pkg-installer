#include "pkgmaster/core/queue_store.hpp"
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace pkgmaster {
namespace {
std::string hex_encode(const std::string& value) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(value.size() * 2);
    for (unsigned char c : value) {
        out.push_back(digits[c >> 4]);
        out.push_back(digits[c & 15]);
    }
    return out;
}
int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
bool hex_decode(const std::string& input, std::string& output) {
    if (input.size() % 2) return false;
    std::string decoded;
    decoded.reserve(input.size() / 2);
    for (std::size_t i = 0; i < input.size(); i += 2) {
        const int hi = hex_value(input[i]), lo = hex_value(input[i + 1]);
        if (hi < 0 || lo < 0) return false;
        decoded.push_back(static_cast<char>((hi << 4) | lo));
    }
    output = std::move(decoded);
    return true;
}
bool parse_u64(const std::string& value, std::uint64_t& result) {
    if (value.empty()) return false;
    std::uint64_t parsed = 0;
    for (char c : value) {
        if (c < '0' || c > '9') return false;
        const auto digit = static_cast<unsigned>(c - '0');
        if (parsed > (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
            return false;
        parsed = parsed * 10 + digit;
    }
    result = parsed;
    return true;
}
bool valid_state(unsigned value) {
    return value <= static_cast<unsigned>(JobState::Cancelled);
}
bool split_record(const std::string& line, std::vector<std::string>& fields) {
    fields.clear();
    std::size_t start = 0;
    for (;;) {
        const auto tab = line.find('\t', start);
        fields.push_back(line.substr(start, tab == std::string::npos
                                               ? tab : tab - start));
        if (tab == std::string::npos) break;
        start = tab + 1;
    }
    return fields.size() == 8;
}
}

bool QueueStore::save(const std::string& path, const std::vector<Job>& jobs,
                      std::string& error) {
    error.clear();
    if (path.empty()) { error = "Queue path is empty"; return false; }
    const std::string temporary = path + ".tmp";
    std::vector<std::uint64_t> seen_ids;
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) { error = "Cannot open temporary queue file"; return false; }
        out << "PKGMASTER_QUEUE\t1\n";
        for (const auto& job : jobs) {
            const auto state = static_cast<unsigned>(job.state);
            bool duplicate_id = false;
            for (const auto previous : seen_ids) duplicate_id = duplicate_id || previous == job.id;
            if (!job.id || duplicate_id || !valid_state(state) ||
                (job.expected_bytes && job.received_bytes > job.expected_bytes)) {
                error = "Queue contains an invalid job";
                out.close();
                std::remove(temporary.c_str());
                return false;
            }
            seen_ids.push_back(job.id);
            out << job.id << '\t' << job.expected_bytes << '\t'
                << job.received_bytes << '\t' << state << '\t'
                << hex_encode(job.source_url) << '\t'
                << hex_encode(job.file_name) << '\t'
                << hex_encode(job.local_path) << '\t'
                << hex_encode(job.error) << '\n';
        }
        out.flush();
        if (!out) {
            error = "Failed while writing queue snapshot";
            out.close();
            std::remove(temporary.c_str());
            return false;
        }
    }
    // On the PS4/POSIX target rename within the same filesystem atomically
    // replaces the previous snapshot. A failed rename leaves the old file intact.
    if (std::rename(temporary.c_str(), path.c_str()) != 0) {
        error = "Could not atomically replace queue snapshot";
        std::remove(temporary.c_str());
        return false;
    }
    return true;
}

bool QueueStore::load(const std::string& path, std::vector<Job>& jobs,
                      std::string& error) {
    error.clear();
    std::ifstream in(path, std::ios::binary);
    if (!in) { error = "Cannot open queue snapshot"; return false; }
    std::string header;
    if (!std::getline(in, header) || header != "PKGMASTER_QUEUE\t1") {
        error = "Unsupported or corrupt queue snapshot header";
        return false;
    }
    std::vector<Job> parsed;
    std::string line;
    std::size_t line_number = 1;
    while (std::getline(in, line)) {
        ++line_number;
        if (line.empty()) continue;
        std::vector<std::string> fields;
        Job job;
        std::uint64_t state = 0;
        if (!split_record(line, fields) ||
            !parse_u64(fields[0], job.id) || !job.id ||
            !parse_u64(fields[1], job.expected_bytes) ||
            !parse_u64(fields[2], job.received_bytes) ||
            !parse_u64(fields[3], state) || !valid_state(static_cast<unsigned>(state)) ||
            state > std::numeric_limits<unsigned>::max() ||
            (job.expected_bytes && job.received_bytes > job.expected_bytes) ||
            !hex_decode(fields[4], job.source_url) ||
            !hex_decode(fields[5], job.file_name) ||
            !hex_decode(fields[6], job.local_path) ||
            !hex_decode(fields[7], job.error)) {
            error = "Invalid queue record at line " + std::to_string(line_number);
            return false;
        }
        job.state = static_cast<JobState>(state);
        for (const auto& existing : parsed) {
            if (existing.id == job.id) {
                error = "Duplicate job ID at line " + std::to_string(line_number);
                return false;
            }
        }
        parsed.push_back(std::move(job));
    }
    if (in.bad()) { error = "Failed while reading queue snapshot"; return false; }
    jobs = std::move(parsed);
    return true;
}
}
