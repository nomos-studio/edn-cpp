// SPDX-License-Identifier: BSL-1.0
#include <catch2/catch_test_macros.hpp>
#include <edn/value.hpp>

using namespace edn;

TEST_CASE("nil value", "[value]") {
    value v;
    CHECK(v.is_nil());
    CHECK(v.is<std::monostate>());
    CHECK(!v.is<bool>());
}

TEST_CASE("bool value", "[value]") {
    value t = true;
    value f = false;
    CHECK(t.is<bool>());
    CHECK(t.get<bool>() == true);
    CHECK(f.get<bool>() == false);
    CHECK(t != f);
}

TEST_CASE("integer value", "[value]") {
    value v = int64_t{42};
    CHECK(v.is<int64_t>());
    CHECK(v.get<int64_t>() == 42);
    CHECK(value(int64_t{1}) < value(int64_t{2}));
}

TEST_CASE("double value", "[value]") {
    value v = 3.14;
    CHECK(v.is<double>());
    CHECK(v.get<double>() == 3.14);
}

TEST_CASE("string value", "[value]") {
    value v = std::string("hello");
    CHECK(v.is<std::string>());
    CHECK(v.get<std::string>() == "hello");
}

TEST_CASE("keyword", "[value]") {
    keyword k("foo/bar");
    CHECK(k.to_string() == ":foo/bar");
    CHECK(std::string(k.name) == "foo/bar");
    // Interning: two keywords with the same name share the same backing pointer
    keyword k2("foo/bar");
    CHECK(k.name.data() == k2.name.data());
}

TEST_CASE("symbol", "[value]") {
    symbol s("my-sym");
    CHECK(s.to_string() == "my-sym");
    CHECK(std::string(s.name) == "my-sym");
}

TEST_CASE("rational", "[value]") {
    rational r(2, 4);
    CHECK(r.numerator == 1);
    CHECK(r.denominator == 2);
    CHECK(r.to_string() == "1/2");
    CHECK(r.to_double() == 0.5);
    CHECK(rational(1, 3) < rational(1, 2));
    CHECK(rational(0, 1) < rational(1, 3));
}

TEST_CASE("rational negative denominator normalised", "[value]") {
    rational r(1, -2);
    CHECK(r.numerator == -1);
    CHECK(r.denominator == 2);
}

TEST_CASE("bigint", "[value]") {
    bigint b{"123456789012345678901234567890"};
    CHECK(b.to_string() == "123456789012345678901234567890N");
}

TEST_CASE("bigdec", "[value]") {
    bigdec b{"3.14159265358979323846"};
    CHECK(b.to_string() == "3.14159265358979323846M");
}

TEST_CASE("character", "[value]") {
    CHECK(character{'\n'}.to_string() == "\\newline");
    CHECK(character{' '}.to_string() == "\\space");
    CHECK(character{'\t'}.to_string() == "\\tab");
    CHECK(character{'a'}.to_string() == "\\a");
    CHECK(character{0x1234}.to_string() == "\\u1234");
}

TEST_CASE("list", "[value]") {
    list l;
    l.items.push_back(value(int64_t{1}));
    l.items.push_back(value(int64_t{2}));
    CHECK(l.items.size() == 2);
}

TEST_CASE("vector", "[value]") {
    edn::vector v;
    v.items.push_back(value(std::string("a")));
    v.items.push_back(value(std::string("b")));
    CHECK(v.items.size() == 2);
}

TEST_CASE("map insert and find", "[value]") {
    map m;
    m.insert(value(keyword("a")), value(int64_t{1}));
    m.insert(value(keyword("b")), value(int64_t{2}));
    CHECK(m.size() == 2);

    const value* v = m.find(value(keyword("a")));
    REQUIRE(v != nullptr);
    CHECK(v->get<int64_t>() == 1);

    CHECK(m.find_kw("b") != nullptr);
    CHECK(m.find_kw("b")->get<int64_t>() == 2);
    CHECK(m.find_kw("c") == nullptr);
}

TEST_CASE("map replaces on duplicate key", "[value]") {
    map m;
    m.insert(value(keyword("k")), value(int64_t{1}));
    m.insert(value(keyword("k")), value(int64_t{99}));
    CHECK(m.size() == 1);
    CHECK(m.find_kw("k")->get<int64_t>() == 99);
}

TEST_CASE("set insert and contains", "[value]") {
    edn::set s;
    s.insert(value(int64_t{3}));
    s.insert(value(int64_t{1}));
    s.insert(value(int64_t{2}));
    s.insert(value(int64_t{1})); // duplicate
    CHECK(s.size() == 3);
    CHECK(s.contains(value(int64_t{2})));
    CHECK(!s.contains(value(int64_t{4})));
    // Items should be in sorted order
    auto it = s.begin();
    CHECK(it->get<int64_t>() == 1);
    ++it;
    CHECK(it->get<int64_t>() == 2);
    ++it;
    CHECK(it->get<int64_t>() == 3);
}

TEST_CASE("tagged", "[value]") {
    tagged t("mytag", value(int64_t{42}));
    CHECK(t.tag == "mytag");
    REQUIRE(t.val != nullptr);
    CHECK(t.val->get<int64_t>() == 42);
}

TEST_CASE("tagged copy", "[value]") {
    tagged orig("t", value(std::string("hello")));
    tagged copy = orig;
    CHECK(copy.tag == "t");
    REQUIRE(copy.val != nullptr);
    CHECK(copy.val->get<std::string>() == "hello");
    // Verify deep copy
    *orig.val = value(std::string("changed"));
    CHECK(copy.val->get<std::string>() == "hello");
}

TEST_CASE("value_less type rank", "[value]") {
    value_less less;
    // nil < bool < int < double < rational < bigint < bigdec < string < char
    //   < keyword < symbol < vector < list < map < set < tagged
    value nil_v;
    value bool_v = false;
    value int_v  = int64_t{0};
    value dbl_v  = 0.0;
    value rat_v  = rational{0, 1};
    value bi_v   = bigint{"0"};
    value bd_v   = bigdec{"0"};
    value str_v  = std::string{};
    value char_v = character{0};
    value kw_v   = keyword{"a"};
    value sym_v  = symbol{"a"};
    value vec_v  = edn::vector{};
    value lst_v  = edn::list{};
    value map_v  = edn::map{};
    value set_v  = edn::set{};
    value tag_v  = tagged{"t", value{}};

    CHECK(less(nil_v, bool_v));
    CHECK(less(bool_v, int_v));
    CHECK(less(int_v, dbl_v));
    CHECK(less(dbl_v, rat_v));
    CHECK(less(rat_v, bi_v));
    CHECK(less(bi_v, bd_v));
    CHECK(less(bd_v, str_v));
    CHECK(less(str_v, char_v));
    CHECK(less(char_v, kw_v));
    CHECK(less(kw_v, sym_v));
    CHECK(less(sym_v, vec_v));
    CHECK(less(vec_v, lst_v));
    CHECK(less(lst_v, map_v));
    CHECK(less(map_v, set_v));
    CHECK(less(set_v, tag_v));
}
