// SPDX-License-Identifier: BSL-1.0
#include <catch2/catch_test_macros.hpp>
#include <edn/emitter.hpp>
#include <edn/parser.hpp>

using namespace edn;

TEST_CASE("parse nil", "[parser]") {
    auto r = parse("nil");
    REQUIRE(r);
    CHECK(r->is_nil());
}

TEST_CASE("parse bool", "[parser]") {
    CHECK(parse("true")->get<bool>() == true);
    CHECK(parse("false")->get<bool>() == false);
}

TEST_CASE("parse integer", "[parser]") {
    CHECK(parse("42")->get<int64_t>() == 42);
    CHECK(parse("-1")->get<int64_t>() == -1);
    CHECK(parse("0")->get<int64_t>() == 0);
}

TEST_CASE("parse float", "[parser]") {
    CHECK(parse("3.14")->get<double>() == 3.14);
    CHECK(parse("-1.0")->get<double>() == -1.0);
    CHECK(parse("1e10")->get<double>() == 1e10);
}

TEST_CASE("parse rational", "[parser]") {
    auto r = parse("1/3");
    REQUIRE(r);
    auto& rat = r->get<rational>();
    CHECK(rat.numerator == 1);
    CHECK(rat.denominator == 3);
}

TEST_CASE("parse bigint", "[parser]") {
    auto r = parse("123N");
    REQUIRE(r);
    CHECK(r->get<bigint>().digits == "123");
}

TEST_CASE("parse bigdec", "[parser]") {
    auto r = parse("3.14M");
    REQUIRE(r);
    CHECK(r->get<bigdec>().digits == "3.14");
}

TEST_CASE("parse string", "[parser]") {
    CHECK(parse(R"("hello")")->get<std::string>() == "hello");
    CHECK(parse(R"("a\nb")")->get<std::string>() == "a\nb");
    CHECK(parse(R"("a\"b")")->get<std::string>() == "a\"b");
    CHECK(parse(R"("a\\b")")->get<std::string>() == "a\\b");
}

TEST_CASE("parse keyword", "[parser]") {
    auto r = parse(":foo");
    REQUIRE(r);
    CHECK(std::string(r->get<keyword>().name) == "foo");
    CHECK(r->get<keyword>().to_string() == ":foo");
}

TEST_CASE("parse namespaced keyword", "[parser]") {
    auto r = parse(":foo/bar");
    REQUIRE(r);
    CHECK(std::string(r->get<keyword>().name) == "foo/bar");
}

TEST_CASE("parse symbol", "[parser]") {
    auto r = parse("my-sym");
    REQUIRE(r);
    CHECK(std::string(r->get<symbol>().name) == "my-sym");
}

TEST_CASE("parse character literals", "[parser]") {
    CHECK(parse(R"(\a)")->get<character>().codepoint == 'a');
    CHECK(parse(R"(\newline)")->get<character>().codepoint == '\n');
    CHECK(parse(R"(\space)")->get<character>().codepoint == ' ');
    CHECK(parse(R"(\tab)")->get<character>().codepoint == '\t');
}

TEST_CASE("parse list", "[parser]") {
    auto r = parse("(1 2 3)");
    REQUIRE(r);
    const auto& l = r->get<list>();
    REQUIRE(l.items.size() == 3);
    CHECK(l.items[0].get<int64_t>() == 1);
    CHECK(l.items[1].get<int64_t>() == 2);
    CHECK(l.items[2].get<int64_t>() == 3);
}

TEST_CASE("parse vector", "[parser]") {
    auto r = parse("[1 2 3]");
    REQUIRE(r);
    const auto& v = r->get<edn::vector>();
    REQUIRE(v.items.size() == 3);
    CHECK(v.items[0].get<int64_t>() == 1);
}

TEST_CASE("parse map", "[parser]") {
    auto r = parse("{:a 1 :b 2}");
    REQUIRE(r);
    const auto& m = r->get<edn::map>();
    CHECK(m.size() == 2);
    REQUIRE(m.find_kw("a") != nullptr);
    CHECK(m.find_kw("a")->get<int64_t>() == 1);
    REQUIRE(m.find_kw("b") != nullptr);
    CHECK(m.find_kw("b")->get<int64_t>() == 2);
}

TEST_CASE("parse set", "[parser]") {
    auto r = parse("#{1 2 3}");
    REQUIRE(r);
    const auto& s = r->get<edn::set>();
    CHECK(s.size() == 3);
    CHECK(s.contains(value(int64_t{2})));
}

TEST_CASE("parse tagged literal", "[parser]") {
    auto r = parse("#myns/tag 42");
    REQUIRE(r);
    const auto& t = r->get<tagged>();
    CHECK(t.tag == "myns/tag");
    REQUIRE(t.val != nullptr);
    CHECK(t.val->get<int64_t>() == 42);
}

TEST_CASE("parse discard (#_)", "[parser]") {
    auto r = parse("#_ ignored 42");
    REQUIRE(r);
    CHECK(r->get<int64_t>() == 42);
}

TEST_CASE("parse comment", "[parser]") {
    auto r = parse("; this is a comment\n42");
    REQUIRE(r);
    CHECK(r->get<int64_t>() == 42);
}

TEST_CASE("parse commas as whitespace", "[parser]") {
    auto r = parse("{:a 1, :b 2}");
    REQUIRE(r);
    CHECK(r->get<edn::map>().size() == 2);
}

TEST_CASE("parse nested structures", "[parser]") {
    auto r = parse("{:a [1 2 3] :b {:c true}}");
    REQUIRE(r);
    const auto& m = r->get<edn::map>();
    REQUIRE(m.find_kw("a") != nullptr);
    CHECK(m.find_kw("a")->get<edn::vector>().items.size() == 3);
    REQUIRE(m.find_kw("b") != nullptr);
    REQUIRE(m.find_kw("b")->get<edn::map>().find_kw("c") != nullptr);
    CHECK(m.find_kw("b")->get<edn::map>().find_kw("c")->get<bool>() == true);
}

TEST_CASE("parse error on unterminated list", "[parser]") {
    auto r = parse("(1 2");
    CHECK(!r);
    CHECK(r.error().message.find("unterminated") != std::string::npos);
}

TEST_CASE("parse error on trailing input", "[parser]") {
    auto r = parse("42 extra");
    CHECK(!r);
}

TEST_CASE("parse error on odd-length map", "[parser]") {
    auto r = parse("{:a}");
    CHECK(!r);
}

TEST_CASE("custom tag handler", "[parser]") {
    parser_opts opts;
    opts.tag_handlers.emplace_back("upper", [](value v) -> value {
        std::string s = v.get<std::string>();
        for (char& c : s)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return value(std::move(s));
    });
    auto r = parse(R"(#upper "hello")", opts);
    REQUIRE(r);
    CHECK(r->get<std::string>() == "HELLO");
}
