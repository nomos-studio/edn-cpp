// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace edn {

// ---------------------------------------------------------------------------
// Intern table — keyword and symbol names have program lifetime
// ---------------------------------------------------------------------------

namespace intern {
    // Returns a string_view into the intern table; valid for program lifetime.
    // Thread-safe.
    std::string_view get(std::string_view s);
} // namespace intern

// ---------------------------------------------------------------------------
// Leaf types — no dependency on edn::value
// ---------------------------------------------------------------------------

struct character {
    char32_t    codepoint{};
    std::string to_string() const;
    bool        operator==(const character& o) const noexcept { return codepoint == o.codepoint; }
    bool        operator<(const character& o) const noexcept { return codepoint < o.codepoint; }
};

struct keyword {
    std::string_view name; // interned; program lifetime
    explicit keyword(std::string_view s) : name(intern::get(s)) {}
    std::string to_string() const { return ':' + std::string(name); }
    bool        operator==(const keyword& o) const noexcept { return name == o.name; }
    bool        operator<(const keyword& o) const noexcept { return name < o.name; }
};

struct symbol {
    std::string_view name; // interned; program lifetime
    explicit symbol(std::string_view s) : name(intern::get(s)) {}
    std::string to_string() const { return std::string(name); }
    bool        operator==(const symbol& o) const noexcept { return name == o.name; }
    bool        operator<(const symbol& o) const noexcept { return name < o.name; }
};

struct rational {
    int64_t numerator{0};
    int64_t denominator{1};         // always positive; sign lives in numerator
    rational(int64_t n, int64_t d); // reduces via std::gcd
    std::string to_string() const;
    double      to_double() const { return static_cast<double>(numerator) / denominator; }
    bool        operator==(const rational& o) const noexcept {
        return numerator == o.numerator && denominator == o.denominator;
    }
    bool operator<(const rational& o) const noexcept {
        // cross-multiply: a/b < c/d ↔ a*d < c*b (denominators always positive)
        return numerator * o.denominator < o.numerator * denominator;
    }
};

struct bigint {
    std::string digits; // numeric string without N suffix
    std::string to_string() const { return digits + 'N'; }
    bool        operator==(const bigint& o) const noexcept { return digits == o.digits; }
    bool        operator<(const bigint& o) const noexcept { return digits < o.digits; }
};

struct bigdec {
    std::string digits; // numeric string without M suffix
    std::string to_string() const { return digits + 'M'; }
    bool        operator==(const bigdec& o) const noexcept { return digits == o.digits; }
    bool        operator<(const bigdec& o) const noexcept { return digits < o.digits; }
};

// ---------------------------------------------------------------------------
// Forward declaration — required by compound type data members
// ---------------------------------------------------------------------------

class value;

// ---------------------------------------------------------------------------
// Compound types
//
// Data members use std::vector<value> / std::vector<std::pair<value,value>>.
// On libc++ (Apple Clang), member functions that call into vector cannot be
// defined inline while `value` is incomplete — ALL member function bodies are
// therefore out-of-line (defined in value.cpp where `value` is complete).
// ---------------------------------------------------------------------------

struct list {
    std::vector<value> items;

    std::string to_string() const;
    bool        operator==(const list& o) const noexcept;
    bool        operator<(const list& o) const noexcept;
};

struct vector {
    std::vector<value> items;

    std::string to_string() const;
    bool        operator==(const vector& o) const noexcept;
    bool        operator<(const vector& o) const noexcept;
};

// map: sorted-vector (flat map) backing; minimal surface area per handoff.
struct map {
    std::vector<std::pair<value, value>> entries; // sorted by key via value_less

    void         insert(value key, value val);
    const value* find(const value& key) const;
    bool         contains(const value& key) const;
    const value* find_kw(std::string_view name) const;

    std::size_t size() const noexcept;
    bool        empty() const noexcept;
    // begin/end return iterators over entries; implementation in value.cpp
    auto begin() const;
    auto end() const;

    std::string to_string() const;
    bool        operator==(const map& o) const noexcept;
    bool        operator<(const map& o) const noexcept;
};

struct set {
    std::vector<value> items; // sorted via value_less; no duplicates

    void insert(value v);
    bool contains(const value& v) const;

    std::size_t size() const noexcept;
    bool        empty() const noexcept;
    auto        begin() const;
    auto        end() const;

    std::string to_string() const;
    bool        operator==(const set& o) const noexcept;
    bool        operator<(const set& o) const noexcept;
};

// tagged: heap-allocates its single recursive value to break the size cycle.
struct tagged {
    std::string            tag;
    std::unique_ptr<value> val;

    tagged(std::string t, value v);
    tagged(const tagged& o);
    tagged& operator=(const tagged& o);
    tagged(tagged&&) noexcept            = default;
    tagged& operator=(tagged&&) noexcept = default;

    std::string to_string() const;
    bool        operator==(const tagged& o) const noexcept;
    bool        operator<(const tagged& o) const noexcept;
};

// ---------------------------------------------------------------------------
// edn::value
//
// Variant arms ordered to match the canonical total ordering so that
// variant::index() doubles as type rank — no separate rank table.
//
// Rank:  nil(0) < bool(1) < int64(2) < double(3) < rational(4) < bigint(5)
//      < bigdec(6) < string(7) < character(8) < keyword(9) < symbol(10)
//      < vector(11) < list(12) < map(13) < set(14) < tagged(15)
// ---------------------------------------------------------------------------

class value {
  public:
    using variant_t = std::variant<std::monostate, //  0  nil
                                   bool,           //  1
                                   int64_t,        //  2
                                   double,         //  3
                                   rational,       //  4
                                   bigint,         //  5
                                   bigdec,         //  6
                                   std::string,    //  7
                                   character,      //  8
                                   keyword,        //  9
                                   symbol,         // 10
                                   vector,         // 11
                                   list,           // 12
                                   map,            // 13
                                   set,            // 14
                                   tagged          // 15
                                   >;

    value() noexcept : v_(std::monostate{}) {}

    /* implicit */ value(std::monostate) noexcept : v_(std::monostate{}) {}
    /* implicit */ value(bool b) noexcept : v_(b) {}
    /* implicit */ value(int64_t i) noexcept : v_(i) {}
    /* implicit */ value(int i) noexcept : v_(static_cast<int64_t>(i)) {}
    /* implicit */ value(double d) noexcept : v_(d) {}
    /* implicit */ value(std::string s) : v_(std::move(s)) {}
    /* implicit */ value(const char* s) : v_(std::string(s)) {}
    /* implicit */ value(character c) noexcept : v_(c) {}
    /* implicit */ value(keyword k) noexcept : v_(k) {}
    /* implicit */ value(symbol s) noexcept : v_(s) {}
    /* implicit */ value(rational r) noexcept : v_(r) {}
    /* implicit */ value(bigint b) : v_(std::move(b)) {}
    /* implicit */ value(bigdec b) : v_(std::move(b)) {}
    /* implicit */ value(list l) : v_(std::move(l)) {}
    /* implicit */ value(vector v) : v_(std::move(v)) {}
    /* implicit */ value(map m) : v_(std::move(m)) {}
    /* implicit */ value(set s) : v_(std::move(s)) {}
    /* implicit */ value(tagged t) : v_(std::move(t)) {}

    template <typename T> bool is() const noexcept { return std::holds_alternative<T>(v_); }

    bool is_nil() const noexcept { return is<std::monostate>(); }
    bool is_bool() const noexcept { return is<bool>(); }
    bool is_int() const noexcept { return is<int64_t>(); }

    template <typename T> const T& get() const { return std::get<T>(v_); }

    template <typename T> T& get() { return std::get<T>(v_); }

    const variant_t& as_variant() const noexcept { return v_; }
    variant_t&       as_variant() noexcept { return v_; }

    bool operator==(const value& o) const noexcept { return v_ == o.v_; }
    bool operator!=(const value& o) const noexcept { return v_ != o.v_; }
    bool operator<(const value& o) const noexcept;

  private:
    variant_t v_;
};

// ---------------------------------------------------------------------------
// Deferred out-of-line definitions for compound type iterators.
// These must come after `class value` is complete.
// ---------------------------------------------------------------------------

inline auto map::begin() const {
    return entries.begin();
}
inline auto map::end() const {
    return entries.end();
}
inline std::size_t map::size() const noexcept {
    return entries.size();
}
inline bool map::empty() const noexcept {
    return entries.empty();
}

inline auto set::begin() const {
    return items.begin();
}
inline auto set::end() const {
    return items.end();
}
inline std::size_t set::size() const noexcept {
    return items.size();
}
inline bool set::empty() const noexcept {
    return items.empty();
}

// ---------------------------------------------------------------------------
// value_less — canonical total ordering for sorted containers
// ---------------------------------------------------------------------------

struct value_less {
    bool operator()(const value& a, const value& b) const noexcept;
};

// ---------------------------------------------------------------------------
// to_string free function — compact EDN emission
// ---------------------------------------------------------------------------

std::string to_string(const value& v);

} // namespace edn
