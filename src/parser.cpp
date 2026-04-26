// SPDX-License-Identifier: BSL-1.0
#include <edn/parser.hpp>

#include <cassert>
#include <cctype>
#include <charconv>
#include <cstring>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace edn {

// ---------------------------------------------------------------------------
// parse_error helpers
// ---------------------------------------------------------------------------

std::string parse_error::to_string() const {
    return message + " (line " + std::to_string(line) + ", col " + std::to_string(column) + ')';
}

// ---------------------------------------------------------------------------
// Cursor — tracks position through a string_view
// ---------------------------------------------------------------------------

struct cursor {
    std::string_view   src;
    std::size_t        pos{0};
    int                line{1};
    int                col{1};
    const parser_opts& opts;

    explicit cursor(std::string_view s, const parser_opts& o) : src(s), opts(o) {}

    bool at_end() const noexcept { return pos >= src.size(); }
    char peek() const noexcept { return at_end() ? '\0' : src[pos]; }
    char peek2() const noexcept { return (pos + 1 < src.size()) ? src[pos + 1] : '\0'; }

    char advance() {
        char c = src[pos++];
        if (c == '\n') {
            ++line;
            col = 1;
        } else {
            ++col;
        }
        return c;
    }

    bool match(char c) {
        if (!at_end() && src[pos] == c) {
            advance();
            return true;
        }
        return false;
    }

    parse_error make_error(std::string msg) const {
        return parse_error{std::move(msg), line, col, pos};
    }

    void skip_ws() {
        while (!at_end()) {
            char c = peek();
            if (c == ',' || c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                advance();
            } else if (c == ';') {
                while (!at_end() && peek() != '\n')
                    advance();
            } else {
                break;
            }
        }
    }
};

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

static result<value> parse_value(cursor& cur);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool is_symbol_start(char c) {
    // EDN symbols can start with: letter, ., *, +, !, -, _, ?, $, %, &, =, <, >, /
    return std::isalpha(static_cast<unsigned char>(c)) || c == '.' || c == '*' || c == '+' ||
           c == '!' || c == '_' || c == '?' || c == '$' || c == '%' || c == '&' || c == '=' ||
           c == '<' || c == '>' || c == '/';
}

static bool is_symbol_continue(char c) {
    return is_symbol_start(c) || std::isdigit(static_cast<unsigned char>(c)) || c == '-' ||
           c == '#' || c == '\'';
}

static bool is_number_start(char c, char next) {
    if (std::isdigit(static_cast<unsigned char>(c)))
        return true;
    if ((c == '-' || c == '+') && std::isdigit(static_cast<unsigned char>(next)))
        return true;
    return false;
}

// ---------------------------------------------------------------------------
// String parsing
// ---------------------------------------------------------------------------

static result<value> parse_string(cursor& cur) {
    assert(cur.peek() == '"');
    cur.advance();

    std::string out;
    while (!cur.at_end() && cur.peek() != '"') {
        char c = cur.advance();
        if (c != '\\') {
            out += c;
            continue;
        }
        if (cur.at_end())
            return result<value>(cur.make_error("unterminated string escape"));
        char esc = cur.advance();
        switch (esc) {
        case '"':
            out += '"';
            break;
        case '\\':
            out += '\\';
            break;
        case '/':
            out += '/';
            break;
        case 'b':
            out += '\b';
            break;
        case 'f':
            out += '\f';
            break;
        case 'n':
            out += '\n';
            break;
        case 'r':
            out += '\r';
            break;
        case 't':
            out += '\t';
            break;
        case 'u': {
            if (cur.pos + 4 > cur.src.size())
                return result<value>(cur.make_error("incomplete \\uNNNN escape"));
            char hex[5] = {};
            for (int i = 0; i < 4; ++i)
                hex[i] = cur.advance();
            unsigned long cp = std::strtoul(hex, nullptr, 16);
            // Encode as UTF-8
            if (cp < 0x80) {
                out += static_cast<char>(cp);
            } else if (cp < 0x800) {
                out += static_cast<char>(0xC0 | (cp >> 6));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            } else {
                out += static_cast<char>(0xE0 | (cp >> 12));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
            break;
        }
        default:
            return result<value>(cur.make_error(std::string("unknown escape \\") + esc));
        }
    }
    if (!cur.match('"'))
        return result<value>(cur.make_error("unterminated string"));
    return result<value>(value(std::move(out)));
}

// ---------------------------------------------------------------------------
// Character parsing
// ---------------------------------------------------------------------------

static result<value> parse_character(cursor& cur) {
    assert(cur.peek() == '\\');
    cur.advance();

    if (cur.at_end())
        return result<value>(cur.make_error("unexpected end after \\"));

    // Collect the character token (up to next delimiter)
    std::string tok;
    tok += cur.advance();
    while (!cur.at_end() && is_symbol_continue(cur.peek()))
        tok += cur.advance();

    // Named characters
    if (tok == "newline")
        return result<value>(value(character{'\n'}));
    if (tok == "space")
        return result<value>(value(character{' '}));
    if (tok == "tab")
        return result<value>(value(character{'\t'}));
    if (tok == "return")
        return result<value>(value(character{'\r'}));
    if (tok == "backspace")
        return result<value>(value(character{'\b'}));
    if (tok == "formfeed")
        return result<value>(value(character{'\f'}));

    // \uNNNN
    if (tok.size() == 5 && tok[0] == 'u') {
        unsigned long cp = std::strtoul(tok.c_str() + 1, nullptr, 16);
        return result<value>(value(character{static_cast<char32_t>(cp)}));
    }

    // Single character
    if (tok.size() == 1)
        return result<value>(value(character{static_cast<char32_t>(tok[0])}));

    return result<value>(cur.make_error("unknown character literal: \\" + tok));
}

// ---------------------------------------------------------------------------
// Number parsing
// ---------------------------------------------------------------------------

static result<value> parse_number(cursor& cur) {
    auto start = cur.pos;

    // Collect the raw token
    if (cur.peek() == '-' || cur.peek() == '+')
        cur.advance();
    while (!cur.at_end() && (std::isdigit(static_cast<unsigned char>(cur.peek())) ||
                             cur.peek() == '.' || cur.peek() == 'e' || cur.peek() == 'E' ||
                             cur.peek() == '+' || cur.peek() == '-' || cur.peek() == '/'))
        cur.advance();

    // Suffix: N (bigint), M (bigdec)
    char suffix = '\0';
    if (!cur.at_end() && (cur.peek() == 'N' || cur.peek() == 'M'))
        suffix = cur.advance();

    std::string_view tok = cur.src.substr(start, cur.pos - start - (suffix ? 1 : 0));

    if (suffix == 'N')
        return result<value>(value(bigint{std::string(tok)}));
    if (suffix == 'M')
        return result<value>(value(bigdec{std::string(tok)}));

    // Rational: contains '/'
    if (auto slash = tok.find('/'); slash != std::string_view::npos) {
        int64_t num = 0, den = 0;
        std::from_chars(tok.data(), tok.data() + slash, num);
        std::from_chars(tok.data() + slash + 1, tok.data() + tok.size(), den);
        if (den == 0)
            return result<value>(cur.make_error("rational denominator is zero"));
        return result<value>(value(rational(num, den)));
    }

    // Float: contains '.' or 'e'/'E'
    // std::from_chars for double requires macOS 26+ in Apple's libc++; use strtod.
    if (tok.find('.') != std::string_view::npos || tok.find('e') != std::string_view::npos ||
        tok.find('E') != std::string_view::npos) {
        std::string buf(tok);
        char*       end{};
        double      d = std::strtod(buf.c_str(), &end);
        if (end != buf.c_str() + buf.size())
            return result<value>(cur.make_error("invalid float: " + buf));
        return result<value>(value(d));
    }

    // Integer
    int64_t i{};
    auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), i);
    if (ec != std::errc{})
        return result<value>(cur.make_error("invalid integer: " + std::string(tok)));
    return result<value>(value(i));
}

// ---------------------------------------------------------------------------
// Symbol / keyword / nil / bool parsing
// ---------------------------------------------------------------------------

static result<value> parse_symbol_or_kw(cursor& cur) {
    bool is_kw = (cur.peek() == ':');
    if (is_kw)
        cur.advance();

    auto start = cur.pos;
    while (!cur.at_end() && is_symbol_continue(cur.peek()))
        cur.advance();

    std::string_view name = cur.src.substr(start, cur.pos - start);
    if (name.empty())
        return result<value>(cur.make_error("empty symbol/keyword name"));

    if (!is_kw) {
        if (name == "nil")
            return result<value>(value(std::monostate{}));
        if (name == "true")
            return result<value>(value(true));
        if (name == "false")
            return result<value>(value(false));
        return result<value>(value(symbol(name)));
    }
    return result<value>(value(keyword(name)));
}

// ---------------------------------------------------------------------------
// Collection parsing
// ---------------------------------------------------------------------------

static result<value> parse_list(cursor& cur) {
    assert(cur.peek() == '(');
    cur.advance();

    list l;
    while (true) {
        cur.skip_ws();
        if (cur.at_end())
            return result<value>(cur.make_error("unterminated list"));
        if (cur.peek() == ')') {
            cur.advance();
            break;
        }
        auto v = parse_value(cur);
        if (!v)
            return v;
        l.items.push_back(std::move(*v));
    }
    return result<value>(value(std::move(l)));
}

static result<value> parse_vector(cursor& cur) {
    assert(cur.peek() == '[');
    cur.advance();

    edn::vector vec;
    while (true) {
        cur.skip_ws();
        if (cur.at_end())
            return result<value>(cur.make_error("unterminated vector"));
        if (cur.peek() == ']') {
            cur.advance();
            break;
        }
        auto v = parse_value(cur);
        if (!v)
            return v;
        vec.items.push_back(std::move(*v));
    }
    return result<value>(value(std::move(vec)));
}

static result<value> parse_map(cursor& cur) {
    assert(cur.peek() == '{');
    cur.advance();

    edn::map m;
    while (true) {
        cur.skip_ws();
        if (cur.at_end())
            return result<value>(cur.make_error("unterminated map"));
        if (cur.peek() == '}') {
            cur.advance();
            break;
        }

        auto k = parse_value(cur);
        if (!k)
            return k;

        cur.skip_ws();
        if (cur.at_end() || cur.peek() == '}')
            return result<value>(cur.make_error("map has odd number of forms"));

        auto v = parse_value(cur);
        if (!v)
            return v;

        m.insert(std::move(*k), std::move(*v));
    }
    return result<value>(value(std::move(m)));
}

// ---------------------------------------------------------------------------
// Dispatch reader (#...)
// ---------------------------------------------------------------------------

static result<value> parse_dispatch(cursor& cur) {
    assert(cur.peek() == '#');
    cur.advance();

    if (cur.at_end())
        return result<value>(cur.make_error("unexpected end after #"));

    char c = cur.peek();

    // Set: #{...}
    if (c == '{') {
        cur.advance();
        edn::set s;
        while (true) {
            cur.skip_ws();
            if (cur.at_end())
                return result<value>(cur.make_error("unterminated set"));
            if (cur.peek() == '}') {
                cur.advance();
                break;
            }
            auto v = parse_value(cur);
            if (!v)
                return v;
            s.insert(std::move(*v));
        }
        return result<value>(value(std::move(s)));
    }

    // Discard: #_ value
    if (c == '_') {
        cur.advance();
        cur.skip_ws();
        auto discarded = parse_value(cur);
        if (!discarded)
            return discarded;
        // Recurse: the next value is what we actually return
        cur.skip_ws();
        return parse_value(cur);
    }

    // Tagged literal: #tag value
    auto start = cur.pos;
    while (!cur.at_end() && is_symbol_continue(cur.peek()))
        cur.advance();
    std::string tag(cur.src.substr(start, cur.pos - start));
    if (tag.empty())
        return result<value>(cur.make_error("empty tag after #"));

    cur.skip_ws();
    auto v = parse_value(cur);
    if (!v)
        return v;

    // Check tag_handlers
    for (const auto& [name, handler] : cur.opts.tag_handlers) {
        if (name == tag)
            return result<value>(handler(std::move(*v)));
    }

    if (cur.opts.preserve_unknown_tags)
        return result<value>(value(tagged(std::move(tag), std::move(*v))));

    return result<value>(cur.make_error("unknown tag: #" + tag));
}

// ---------------------------------------------------------------------------
// Top-level dispatch
// ---------------------------------------------------------------------------

static result<value> parse_value(cursor& cur) {
    cur.skip_ws();
    if (cur.at_end())
        return result<value>(cur.make_error("unexpected end of input"));

    char c = cur.peek();

    if (c == '"')
        return parse_string(cur);
    if (c == '\\')
        return parse_character(cur);
    if (c == '(')
        return parse_list(cur);
    if (c == '[')
        return parse_vector(cur);
    if (c == '{')
        return parse_map(cur);
    if (c == '#')
        return parse_dispatch(cur);
    if (c == ':' || is_symbol_start(c))
        return parse_symbol_or_kw(cur);
    if (is_number_start(c, cur.peek2()))
        return parse_number(cur);
    if ((c == '-' || c == '+') && !is_number_start(c, cur.peek2()))
        return parse_symbol_or_kw(cur); // bare - and + are symbols

    return result<value>(cur.make_error(std::string("unexpected character: '") + c + '\''));
}

// ---------------------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------------------

result<value> parse(std::string_view input, parser_opts opts) {
    cursor cur(input, opts);
    auto   v = parse_value(cur);
    if (!v)
        return v;
    cur.skip_ws();
    if (!cur.at_end())
        return result<value>(cur.make_error("trailing input after value"));
    return v;
}

result<value> parse(std::istream& in, parser_opts opts) {
    std::ostringstream ss;
    ss << in.rdbuf();
    return parse(ss.str(), std::move(opts));
}

} // namespace edn
