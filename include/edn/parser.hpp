// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "value.hpp"

#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

// C++23: std::expected; C++20: thin result wrapper with the same surface area.
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202211L
#include <expected>
#endif

namespace edn {

// ---------------------------------------------------------------------------
// parse_error — carried by result on failure
// ---------------------------------------------------------------------------

struct parse_error {
    std::string message;
    int         line{1};
    int         column{1};
    std::size_t offset{0};

    std::string to_string() const;
};

// ---------------------------------------------------------------------------
// result<T> — no-exception error channel
// ---------------------------------------------------------------------------

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202211L

template <typename T> using result = std::expected<T, parse_error>;

inline parse_error& error_of(result<value>& r) {
    return r.error();
}

#else

template <typename T> class result {
  public:
    result(T v) : ok_(true), val_(std::move(v)), err_{} {}
    result(parse_error e) : ok_(false), val_{}, err_(std::move(e)) {}

    explicit operator bool() const noexcept { return ok_; }
    bool     has_value() const noexcept { return ok_; }

    const T& value() const { return val_; }
    T&       value() { return val_; }

    const parse_error& error() const { return err_; }
    parse_error&       error() { return err_; }

    const T& operator*() const { return val_; }
    T&       operator*() { return val_; }
    const T* operator->() const { return &val_; }
    T*       operator->() { return &val_; }

  private:
    bool        ok_;
    T           val_;
    parse_error err_;
};

#endif

// ---------------------------------------------------------------------------
// tag_handler / tag_dispatch
// ---------------------------------------------------------------------------

using tag_handler  = std::function<value(value)>;
using tag_dispatch = std::vector<std::pair<std::string, tag_handler>>;

// ---------------------------------------------------------------------------
// parser_opts
// ---------------------------------------------------------------------------

struct parser_opts {
    tag_dispatch tag_handlers;
    bool         preserve_unknown_tags = true; // unknown tags → edn::tagged
};

// ---------------------------------------------------------------------------
// parse entry points
// ---------------------------------------------------------------------------

result<value> parse(std::string_view input, parser_opts opts = {});
result<value> parse(std::istream& in, parser_opts opts = {});

} // namespace edn
