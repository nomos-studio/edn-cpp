// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "value.hpp"

#include <iosfwd>
#include <string>

namespace edn {

// ---------------------------------------------------------------------------
// Compact emitter — delegates to per-type to_string()
// ---------------------------------------------------------------------------

std::string to_string(const value& v);
void        write(std::ostream& out, const value& v);

// ---------------------------------------------------------------------------
// Pretty emitter
//
// Algorithm: measure compact form; if it fits within line_width emit inline;
// otherwise break with indented children. Recursive. No Wadler-Lindig in v1.
// ---------------------------------------------------------------------------

struct format_opts {
    int line_width = 80;
    int indent     = 2;
};

std::string pretty_string(const value& v, format_opts opts = {});
void        pretty_print(std::ostream& out, const value& v, format_opts opts = {});

} // namespace edn
