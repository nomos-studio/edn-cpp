// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "parser.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <string>

namespace edn {

// ---------------------------------------------------------------------------
// edn::uuid — canonical 128-bit UUID (consume-only; no generation)
// ---------------------------------------------------------------------------

struct uuid {
    std::array<uint8_t, 16> bytes{};

    std::string to_string() const; // canonical xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx

    bool operator==(const uuid& o) const noexcept { return bytes == o.bytes; }
    bool operator<(const uuid& o) const noexcept { return bytes < o.bytes; }
};

// ---------------------------------------------------------------------------
// Built-in tag handlers — opt-in; not included in default parser_opts
//
// Usage:
//   edn::parser_opts opts;
//   edn::builtins::register_inst(opts);
//   edn::builtins::register_uuid(opts);
//   // or: edn::builtins::register_all(opts);
// ---------------------------------------------------------------------------

namespace builtins {

    // #inst — RFC 3339 string → UTC-normalised system_clock::time_point.
    // UTC offset (e.g. -05:00, Z) is consumed at parse time; time_point stores UTC.
    // Roll-our-own RFC 3339 parser: no std::chrono::parse (uneven stdlib support).
    void register_inst(parser_opts& opts);

    // #uuid — canonical UUID string → edn::uuid (bytes).
    void register_uuid(parser_opts& opts);

    // Both.
    void register_all(parser_opts& opts);

} // namespace builtins
} // namespace edn
