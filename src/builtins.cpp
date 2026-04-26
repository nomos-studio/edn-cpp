// SPDX-License-Identifier: BSL-1.0
#include <edn/builtins.hpp>

#include <charconv>
#include <chrono>
#include <cstdio>
#include <stdexcept>

namespace edn {

// ---------------------------------------------------------------------------
// uuid::to_string
// ---------------------------------------------------------------------------

std::string uuid::to_string() const {
    char buf[37];
    std::snprintf(buf, sizeof(buf),
                  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", bytes[0],
                  bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7], bytes[8],
                  bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
    return buf;
}

// ---------------------------------------------------------------------------
// RFC 3339 parser (~80 lines) — no std::chrono::parse (uneven stdlib support)
//
// Accepts: YYYY-MM-DDTHH:MM:SS[.frac](Z|±HH:MM)
// Returns: UTC microseconds since epoch as int64_t
// ---------------------------------------------------------------------------

namespace {

    static uint8_t parse_hex_byte(const char* p) {
        uint8_t hi = static_cast<uint8_t>(p[0] <= '9' ? p[0] - '0' : (p[0] | 32) - 'a' + 10);
        uint8_t lo = static_cast<uint8_t>(p[1] <= '9' ? p[1] - '0' : (p[1] | 32) - 'a' + 10);
        return static_cast<uint8_t>((hi << 4) | lo);
    }

    static int parse2(const char* p) {
        return (p[0] - '0') * 10 + (p[1] - '0');
    }
    static int parse4(const char* p) {
        return (p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0');
    }

    // Returns true if year is a leap year
    static bool is_leap(int y) {
        return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
    }

    static const int days_in_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // Days since Unix epoch (1970-01-01) for a given date
    static int64_t date_to_days(int y, int m, int d) {
        // Days from 1970-01-01 to start of year y
        int64_t days = 0;
        for (int yr = 1970; yr < y; ++yr)
            days += is_leap(yr) ? 366 : 365;
        for (int mo = 1; mo < m; ++mo) {
            days += days_in_month[mo - 1];
            if (mo == 2 && is_leap(y))
                ++days;
        }
        days += d - 1;
        return days;
    }

    // Parse RFC 3339; return microseconds since epoch (UTC)
    static int64_t parse_rfc3339(const std::string& s) {
        const char* p = s.c_str();
        if (s.size() < 20)
            throw std::invalid_argument("#inst: too short: " + s);

        int year = parse4(p);
        p += 4;
        if (*p++ != '-')
            throw std::invalid_argument("#inst: bad date");
        int month = parse2(p);
        p += 2;
        if (*p++ != '-')
            throw std::invalid_argument("#inst: bad date");
        int day = parse2(p);
        p += 2;
        if (*p++ != 'T' && *(p - 1) != 't')
            throw std::invalid_argument("#inst: expected T");
        int hour = parse2(p);
        p += 2;
        if (*p++ != ':')
            throw std::invalid_argument("#inst: bad time");
        int min = parse2(p);
        p += 2;
        if (*p++ != ':')
            throw std::invalid_argument("#inst: bad time");
        int sec = parse2(p);
        p += 2;

        // Optional fractional seconds
        int64_t frac_us = 0;
        if (*p == '.') {
            ++p;
            const char* frac_start = p;
            while (*p >= '0' && *p <= '9')
                ++p;
            int     frac_len = static_cast<int>(p - frac_start);
            int64_t frac     = 0;
            for (int i = 0; i < frac_len && i < 6; ++i)
                frac = frac * 10 + (frac_start[i] - '0');
            // Pad or truncate to 6 digits (microseconds)
            for (int i = frac_len; i < 6; ++i)
                frac *= 10;
            frac_us = frac;
        }

        // Timezone offset
        int tz_offset_min = 0;
        if (*p == 'Z' || *p == 'z') {
            // UTC
        } else if (*p == '+' || *p == '-') {
            char sign = *p++;
            int  tz_h = parse2(p);
            p += 2;
            if (*p == ':')
                ++p;
            int tz_m      = parse2(p);
            tz_offset_min = (tz_h * 60 + tz_m) * (sign == '+' ? 1 : -1);
        } else {
            throw std::invalid_argument("#inst: missing timezone");
        }

        int64_t days    = date_to_days(year, month, day);
        int64_t epoch_s = days * 86400 + hour * 3600 + min * 60 + sec - tz_offset_min * 60;
        return epoch_s * 1'000'000 + frac_us;
    }

} // namespace

// ---------------------------------------------------------------------------
// builtins::register_inst
// ---------------------------------------------------------------------------

namespace builtins {

    void register_inst(parser_opts& opts) {
        opts.tag_handlers.emplace_back("inst", [](value v) -> value {
            if (!v.is<std::string>())
                throw std::invalid_argument("#inst: expected a string value");
            int64_t us = parse_rfc3339(v.get<std::string>());
            // Store UTC microseconds since epoch as int64_t inside the tagged literal.
            // Consumers extract via: auto& t = v.get<tagged>(); int64_t us = t.val->get<int64_t>();
            return value(tagged("inst", value(us)));
        });
    }

    // ---------------------------------------------------------------------------
    // builtins::register_uuid
    // ---------------------------------------------------------------------------

    void register_uuid(parser_opts& opts) {
        opts.tag_handlers.emplace_back("uuid", [](value v) -> value {
            if (!v.is<std::string>())
                throw std::invalid_argument("#uuid: expected a string value");
            const std::string& s = v.get<std::string>();
            // Canonical form: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx (36 chars)
            if (s.size() != 36 || s[8] != '-' || s[13] != '-' || s[18] != '-' || s[23] != '-')
                throw std::invalid_argument("#uuid: malformed UUID: " + s);

            edn::uuid   u;
            const char* src = s.c_str();
            // 8-4-4-4-12 hex groups, skipping dashes
            int byte_pos = 0;
            for (int i = 0; i < 36; i += (i == 7 || i == 12 || i == 17 || i == 22) ? 3 : 2) {
                if (s[i] == '-') {
                    ++i;
                }
                u.bytes[byte_pos++] = parse_hex_byte(src + i);
            }
            return value(tagged("uuid", value(u.to_string())));
        });
    }

    void register_all(parser_opts& opts) {
        register_inst(opts);
        register_uuid(opts);
    }

} // namespace builtins
} // namespace edn
