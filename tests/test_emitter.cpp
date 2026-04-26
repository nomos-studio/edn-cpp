// SPDX-License-Identifier: BSL-1.0
#include <catch2/catch_test_macros.hpp>
#include <edn/emitter.hpp>
#include <edn/parser.hpp>

using namespace edn;

TEST_CASE("emit nil", "[emitter]") {
    CHECK(to_string(value{}) == "nil");
}

TEST_CASE("emit bool", "[emitter]") {
    CHECK(to_string(value{true}) == "true");
    CHECK(to_string(value{false}) == "false");
}

TEST_CASE("emit integer", "[emitter]") {
    CHECK(to_string(value{int64_t{42}}) == "42");
    CHECK(to_string(value{int64_t{-1}}) == "-1");
    CHECK(to_string(value{int64_t{0}}) == "0");
}

TEST_CASE("emit double", "[emitter]") {
    std::string s = to_string(value{3.14});
    // Must contain decimal point for round-trip fidelity
    CHECK(s.find('.') != std::string::npos);
}

TEST_CASE("emit string with escapes", "[emitter]") {
    CHECK(to_string(value{std::string("hello")}) == R"("hello")");
    CHECK(to_string(value{std::string("a\nb")}) == R"("a\nb")");
    CHECK(to_string(value{std::string("a\"b")}) == R"("a\"b")");
    CHECK(to_string(value{std::string("a\\b")}) == R"("a\\b")");
}

TEST_CASE("emit keyword", "[emitter]") {
    CHECK(to_string(value{keyword{"foo"}}) == ":foo");
    CHECK(to_string(value{keyword{"foo/bar"}}) == ":foo/bar");
}

TEST_CASE("emit symbol", "[emitter]") {
    CHECK(to_string(value{symbol{"my-sym"}}) == "my-sym");
}

TEST_CASE("emit rational", "[emitter]") {
    CHECK(to_string(value{rational{1, 3}}) == "1/3");
}

TEST_CASE("emit bigint", "[emitter]") {
    CHECK(to_string(value{bigint{"99999999999999999999"}}) == "99999999999999999999N");
}

TEST_CASE("emit bigdec", "[emitter]") {
    CHECK(to_string(value{bigdec{"3.14159265358979323846"}}) == "3.14159265358979323846M");
}

TEST_CASE("emit character", "[emitter]") {
    CHECK(to_string(value{character{'\n'}}) == "\\newline");
    CHECK(to_string(value{character{' '}}) == "\\space");
    CHECK(to_string(value{character{'a'}}) == "\\a");
}

TEST_CASE("emit list", "[emitter]") {
    list l;
    l.items.push_back(value{int64_t{1}});
    l.items.push_back(value{int64_t{2}});
    CHECK(to_string(value{l}) == "(1 2)");
}

TEST_CASE("emit vector", "[emitter]") {
    edn::vector v;
    v.items.push_back(value{keyword{"a"}});
    v.items.push_back(value{keyword{"b"}});
    CHECK(to_string(value{v}) == "[:a :b]");
}

TEST_CASE("emit map", "[emitter]") {
    map m;
    m.insert(value{keyword{"a"}}, value{int64_t{1}});
    std::string s = to_string(value{m});
    CHECK(s == "{:a 1}");
}

TEST_CASE("emit set", "[emitter]") {
    edn::set s;
    s.insert(value{int64_t{1}});
    s.insert(value{int64_t{2}});
    CHECK(to_string(value{s}) == "#{1 2}");
}

TEST_CASE("emit tagged", "[emitter]") {
    tagged t("mytag", value{int64_t{42}});
    CHECK(to_string(value{std::move(t)}) == "#mytag 42");
}

TEST_CASE("round-trip simple values via parse+emit", "[emitter]") {
    for (const char* src : {"nil", "true", "false", "42", "-1", ":foo", "my-sym", R"("hello")",
                            "1/3", "[:a :b :c]", "{:x 1}"}) {
        auto r = parse(src);
        REQUIRE(r);
        // Round-trip: emitting the parsed value must re-parse to the same value
        auto r2 = parse(to_string(*r));
        REQUIRE(r2);
        CHECK(*r == *r2);
    }
}

TEST_CASE("pretty_string inline when short", "[emitter]") {
    edn::vector v;
    v.items.push_back(value{int64_t{1}});
    v.items.push_back(value{int64_t{2}});
    // Short vector fits on one line
    std::string s = pretty_string(value{v}, {80, 2});
    CHECK(s.find('\n') == std::string::npos);
    CHECK(s == "[1 2]");
}

TEST_CASE("pretty_string breaks long vectors", "[emitter]") {
    edn::vector v;
    for (int i = 0; i < 10; ++i)
        v.items.push_back(value{std::string(10, 'a' + i)});
    std::string s = pretty_string(value{v}, {20, 2});
    // With line_width=20, this should break
    CHECK(s.find('\n') != std::string::npos);
}
