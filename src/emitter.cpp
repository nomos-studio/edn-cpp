// SPDX-License-Identifier: BSL-1.0
#include <edn/emitter.hpp>

#include <ostream>
#include <sstream>

namespace edn {

// ---------------------------------------------------------------------------
// Compact emitter
// ---------------------------------------------------------------------------

static std::string string_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        default:
            if (c < 0x20) {
                char buf[7];
                std::snprintf(buf, sizeof(buf), "\\u%04X", c);
                out += buf;
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    out += '"';
    return out;
}

std::string to_string(const value& v) {
    return std::visit(
        [](const auto& x) -> std::string {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return "nil";
            } else if constexpr (std::is_same_v<T, bool>) {
                return x ? "true" : "false";
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return std::to_string(x);
            } else if constexpr (std::is_same_v<T, double>) {
                // Clojure-compatible float representation
                std::ostringstream ss;
                ss.precision(17);
                ss << x;
                std::string s = ss.str();
                // Ensure there's a decimal point so it round-trips as a float
                if (s.find('.') == std::string::npos && s.find('e') == std::string::npos &&
                    s.find('E') == std::string::npos) {
                    s += ".0";
                }
                return s;
            } else if constexpr (std::is_same_v<T, std::string>) {
                return string_escape(x);
            } else {
                return x.to_string();
            }
        },
        v.as_variant());
}

void write(std::ostream& out, const value& v) {
    out << to_string(v);
}

// ---------------------------------------------------------------------------
// Container to_string() implementations
// ---------------------------------------------------------------------------

std::string list::to_string() const {
    std::string s = "(";
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i)
            s += ' ';
        s += edn::to_string(items[i]);
    }
    return s + ')';
}

std::string vector::to_string() const {
    std::string s = "[";
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i)
            s += ' ';
        s += edn::to_string(items[i]);
    }
    return s + ']';
}

std::string map::to_string() const {
    std::string s     = "{";
    bool        first = true;
    for (const auto& [k, v] : entries) {
        if (!first)
            s += ", ";
        s += edn::to_string(k) + ' ' + edn::to_string(v);
        first = false;
    }
    return s + '}';
}

std::string set::to_string() const {
    std::string s = "#{";
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i)
            s += ' ';
        s += edn::to_string(items[i]);
    }
    return s + '}';
}

std::string tagged::to_string() const {
    return '#' + tag + ' ' + (val ? edn::to_string(*val) : "nil");
}

// ---------------------------------------------------------------------------
// Pretty emitter
//
// Heuristic: measure compact form; if ≤ line_width, emit inline;
// otherwise break with indented children.
// ---------------------------------------------------------------------------

static std::string pretty_value(const value& v, int depth, const format_opts& opts);

static std::string indent_str(int depth, const format_opts& opts) {
    return std::string(static_cast<std::size_t>(depth * opts.indent), ' ');
}

static std::string pretty_items(const std::vector<value>& items, char open, char close, int depth,
                                const format_opts& opts) {
    // Try compact first
    std::string compact(1, open);
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i)
            compact += ' ';
        compact += edn::to_string(items[i]);
    }
    compact += close;
    if (static_cast<int>(compact.size()) + depth * opts.indent <= opts.line_width)
        return compact;

    // Break: one child per line
    std::string s(1, open);
    std::string ind = indent_str(depth + 1, opts);
    for (std::size_t i = 0; i < items.size(); ++i) {
        s += '\n' + ind + pretty_value(items[i], depth + 1, opts);
    }
    return s + '\n' + indent_str(depth, opts) + close;
}

static std::string pretty_value(const value& v, int depth, const format_opts& opts) {
    return std::visit(
        [&](const auto& x) -> std::string {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, list>) {
                return pretty_items(x.items, '(', ')', depth, opts);
            } else if constexpr (std::is_same_v<T, vector>) {
                return pretty_items(x.items, '[', ']', depth, opts);
            } else if constexpr (std::is_same_v<T, set>) {
                // Build a prefixed version
                auto inner = pretty_items(x.items, '{', '}', depth, opts);
                return '#' + inner;
            } else if constexpr (std::is_same_v<T, map>) {
                // Try compact
                std::string compact = x.to_string();
                if (static_cast<int>(compact.size()) + depth * opts.indent <= opts.line_width)
                    return compact;
                // Break: one kv pair per line
                std::string s     = "{";
                std::string ind   = indent_str(depth + 1, opts);
                bool        first = true;
                for (const auto& [k, val] : x.entries) {
                    if (!first)
                        s += '\n';
                    s += '\n' + ind + pretty_value(k, depth + 1, opts) + ' ' +
                         pretty_value(val, depth + 1, opts);
                    first = false;
                }
                return s + '\n' + indent_str(depth, opts) + '}';
            } else if constexpr (std::is_same_v<T, tagged>) {
                std::string inner = x.val ? pretty_value(*x.val, depth, opts) : "nil";
                return '#' + x.tag + ' ' + inner;
            } else {
                return edn::to_string(v);
            }
        },
        v.as_variant());
}

std::string pretty_string(const value& v, format_opts opts) {
    return pretty_value(v, 0, opts);
}

void pretty_print(std::ostream& out, const value& v, format_opts opts) {
    out << pretty_string(v, std::move(opts));
}

} // namespace edn
