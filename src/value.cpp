// SPDX-License-Identifier: BSL-1.0
#include <edn/emitter.hpp>
#include <edn/value.hpp>

#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace edn {

// ---------------------------------------------------------------------------
// rational
// ---------------------------------------------------------------------------

rational::rational(int64_t n, int64_t d) {
    if (d == 0)
        throw std::domain_error("rational: denominator is zero");
    if (d < 0) {
        n = -n;
        d = -d;
    }
    int64_t g   = std::gcd(std::abs(n), d);
    numerator   = n / g;
    denominator = d / g;
}

std::string rational::to_string() const {
    return std::to_string(numerator) + '/' + std::to_string(denominator);
}

// ---------------------------------------------------------------------------
// character
// ---------------------------------------------------------------------------

std::string character::to_string() const {
    switch (codepoint) {
    case '\n':
        return "\\newline";
    case ' ':
        return "\\space";
    case '\t':
        return "\\tab";
    case '\r':
        return "\\return";
    case '\b':
        return "\\backspace";
    case '\f':
        return "\\formfeed";
    default:
        break;
    }
    if (codepoint > 0xFFFF) {
        // Encode as \uNNNNN+ (non-BMP; surrogate pairs not handled)
        char buf[9]; // \u + up to 6 hex digits (0x10FFFF) + null
        std::snprintf(buf, sizeof(buf), "\\u%04X", static_cast<unsigned>(codepoint));
        return buf;
    }
    if (codepoint > 0x7E || codepoint < 0x20) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "\\u%04X", static_cast<unsigned>(codepoint));
        return buf;
    }
    return std::string("\\") + static_cast<char>(codepoint);
}

// ---------------------------------------------------------------------------
// tagged — copy constructor and assignment
// ---------------------------------------------------------------------------

tagged::tagged(std::string t, value v)
    : tag(std::move(t)), val(std::make_unique<value>(std::move(v))) {
}

tagged::tagged(const tagged& o)
    : tag(o.tag), val(o.val ? std::make_unique<value>(*o.val) : nullptr) {
}

tagged& tagged::operator=(const tagged& o) {
    if (this != &o) {
        tag = o.tag;
        val = o.val ? std::make_unique<value>(*o.val) : nullptr;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// value_less — canonical total ordering
//
// Type rank = variant index (nil=0 < bool=1 < int64=2 < double=3 < rational=4
//   < bigint=5 < bigdec=6 < string=7 < char=8 < keyword=9 < symbol=10
//   < vector=11 < list=12 < map=13 < set=14 < tagged=15)
// ---------------------------------------------------------------------------

bool value_less::operator()(const value& a, const value& b) const noexcept {
    const auto& av = a.as_variant();
    const auto& bv = b.as_variant();

    auto ai = av.index();
    auto bi = bv.index();
    if (ai != bi)
        return ai < bi;

    return std::visit(
        [&]<typename T>(const T& lhs) -> bool {
            if constexpr (std::is_same_v<T, std::monostate>) {
                return false;
            } else if constexpr (std::is_same_v<T, bool>) {
                return lhs < std::get<bool>(bv);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return lhs < std::get<int64_t>(bv);
            } else if constexpr (std::is_same_v<T, double>) {
                return lhs < std::get<double>(bv);
            } else {
                // All remaining types have operator<
                return lhs < std::get<T>(bv);
            }
        },
        av);
}

// ---------------------------------------------------------------------------
// value::operator<
// ---------------------------------------------------------------------------

bool value::operator<(const value& o) const noexcept {
    return value_less{}(*this, o);
}

// ---------------------------------------------------------------------------
// list
// ---------------------------------------------------------------------------

bool list::operator==(const list& o) const noexcept {
    return items == o.items;
}
bool list::operator<(const list& o) const noexcept {
    return std::lexicographical_compare(items.begin(), items.end(), o.items.begin(), o.items.end(),
                                        value_less{});
}

// ---------------------------------------------------------------------------
// vector
// ---------------------------------------------------------------------------

bool vector::operator==(const vector& o) const noexcept {
    return items == o.items;
}
bool vector::operator<(const vector& o) const noexcept {
    return std::lexicographical_compare(items.begin(), items.end(), o.items.begin(), o.items.end(),
                                        value_less{});
}

// ---------------------------------------------------------------------------
// map
// ---------------------------------------------------------------------------

void map::insert(value key, value val) {
    value_less less;
    auto       it = std::lower_bound(entries.begin(), entries.end(), key,
                                     [&less](const auto& e, const value& k) { return less(e.first, k); });
    if (it != entries.end() && !less(key, it->first))
        it->second = std::move(val); // replace existing
    else
        entries.insert(it, {std::move(key), std::move(val)});
}

const value* map::find(const value& key) const {
    value_less less;
    auto       it = std::lower_bound(entries.begin(), entries.end(), key,
                                     [&less](const auto& e, const value& k) { return less(e.first, k); });
    if (it != entries.end() && !less(key, it->first))
        return &it->second;
    return nullptr;
}

bool map::contains(const value& key) const {
    return find(key) != nullptr;
}

const value* map::find_kw(std::string_view name) const {
    return find(value(keyword(name)));
}

bool map::operator==(const map& o) const noexcept {
    return entries == o.entries;
}
bool map::operator<(const map& o) const noexcept {
    return std::lexicographical_compare(entries.begin(), entries.end(), o.entries.begin(),
                                        o.entries.end(), [](const auto& a, const auto& b) {
                                            value_less less;
                                            if (less(a.first, b.first))
                                                return true;
                                            if (less(b.first, a.first))
                                                return false;
                                            return less(a.second, b.second);
                                        });
}

// ---------------------------------------------------------------------------
// set
// ---------------------------------------------------------------------------

void set::insert(value v) {
    value_less less;
    auto       it = std::lower_bound(items.begin(), items.end(), v, less);
    if (it == items.end() || less(v, *it))
        items.insert(it, std::move(v));
    // silently drop duplicates
}

bool set::contains(const value& v) const {
    value_less less;
    auto       it = std::lower_bound(items.begin(), items.end(), v, less);
    return it != items.end() && !less(v, *it);
}

bool set::operator==(const set& o) const noexcept {
    return items == o.items;
}
bool set::operator<(const set& o) const noexcept {
    return std::lexicographical_compare(items.begin(), items.end(), o.items.begin(), o.items.end(),
                                        value_less{});
}

// ---------------------------------------------------------------------------
// tagged
// ---------------------------------------------------------------------------

bool tagged::operator==(const tagged& o) const noexcept {
    if (tag != o.tag)
        return false;
    if (!val && !o.val)
        return true;
    if (!val || !o.val)
        return false;
    return *val == *o.val;
}

bool tagged::operator<(const tagged& o) const noexcept {
    if (tag != o.tag)
        return tag < o.tag;
    if (!val && !o.val)
        return false;
    if (!val)
        return true;
    if (!o.val)
        return false;
    return *val < *o.val;
}

} // namespace edn
