#include "objects/containers/String.h"
#include "catch.hpp"
#include "objects/utility/IteratorAdapters.h"
#include "objects/wrappers/Optional.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("String default construct", "[string]") {
    String s0;
    REQUIRE(s0.size() == 0);
    REQUIRE(s0.empty());
    REQUIRE_SPH_ASSERT(s0[0]);

    REQUIRE(s0.begin() == s0.end());
}

TEST_CASE("String copy construct", "[string]") {
    String s0;
    String s1(s0);
    REQUIRE(s1.size() == 0);
    REQUIRE(s1.empty());

    String s2 = L"test";
    REQUIRE(s2.size() == 4);
    REQUIRE_FALSE(s2.empty());
    REQUIRE(s2[0] == 't');
    REQUIRE(s2[1] == 'e');
    REQUIRE(s2[2] == 's');
    REQUIRE(s2[3] == 't');
    REQUIRE_SPH_ASSERT(s2[4]);

    String s3(s2);
    REQUIRE(s3.size() == 4);
    REQUIRE(s2 == s3);
}

TEST_CASE("String move construct", "[string]") {
    String s1 = L"test";
    String s2(std::move(s1));
    REQUIRE(s2.size() == 4);
    REQUIRE(s2 == L"test");
    REQUIRE(s1.empty());
    REQUIRE(s1 == L"");
}

TEST_CASE("String assign", "[string]") {
    String s1(L"abc");
    s1 = L"test";
    REQUIRE(s1 == L"test");

    String s2(L"other");
    s1 = std::move(s2);
    REQUIRE(s1 == L"other");
    REQUIRE(s2 == L"test"); // Array move swaps content
}

TEST_CASE("String append", "[string]") {
    String s(L"hello");
    s += L' ';
    s += L"world";
    REQUIRE(s == L"hello world");

    String s2(L" again");
    s += s2;
    REQUIRE(s == L"hello world again");
}

TEST_CASE("String toAscii", "[string]") {
    String s(L"test");
    auto c = s.toAscii();
    REQUIRE(strlen(c) == 4);
    REQUIRE(strcmp(c, "test") == 0);
}

TEST_CASE("String iterator", "[string]") {
    String s1(L"hello");
    for (wchar_t& c : s1) {
        c = L'a';
    }
    REQUIRE(s1 == L"aaaaa");

    String s2(L"hello");
    struct Element {
        wchar_t& c1;
        wchar_t& c2;
    };
    for (auto i : iterateTuple<Element>(s1, reverse(s2))) {
        i.c1 = i.c2;
    }
    REQUIRE(s1 == L"olleh");
}

TEST_CASE("String find", "[string]") {
    String s0(L"test");
    REQUIRE(s0.find(L"test") == 0);

    String s1(L"hello world");
    REQUIRE(s1.find(L"hello") == 0);
    REQUIRE(s1.find(L"world") == 6);
    REQUIRE(s1.find(L"o") == 4);
    REQUIRE(s1.find(L"o", 5) == 7);
    REQUIRE(s1.find(L"hello", 1) == String::npos);
    REQUIRE(s1.find(L"hello world2") == String::npos);
    REQUIRE(s1.find(L"test") == String::npos);
    REQUIRE(s1.find(L"rlda") == String::npos);

    String s2(L"aaab");
    REQUIRE(s2.find(L"aaab") == 0);
    REQUIRE(s2.find(L"aab") == 1);
    REQUIRE(s2.find(L"b") == 3);
    REQUIRE(s2.find(L"a") == 0);
    REQUIRE(s2.find(L"a", 2) == 2);

    REQUIRE_SPH_ASSERT(s2.find(L""));
    REQUIRE_SPH_ASSERT(s2.find(L"a", 5));
}

TEST_CASE("String find last", "[string]") {
    String s0(L"test");
    REQUIRE(s0.findLast(L"test") == 0);

    String s1(L"abc abc");
    REQUIRE(s1.findLast(L"abc") == 4);
    REQUIRE(s1.findLast(L"def") == String::npos);
    REQUIRE(s1.findLast(L"abc abc abc") == String::npos);
    REQUIRE_SPH_ASSERT(s1.findLast(L""));

    String s2(L"hello world");
    REQUIRE(s2.findLast(L"l") == 9);
}

TEST_CASE("String replace", "[string]") {
    String s1(L"hello world");
    s1.replace(0, 2, L"a");
    REQUIRE(s1 == L"allo world");
    REQUIRE_SPH_ASSERT(s1.replace(0, 1000, L"test"));
    s1.replace(0, s1.size(), L"test");
    REQUIRE(s1 == L"test");
    s1.replace(0, 1, L"the larg");
    REQUIRE(s1 == L"the largest");

    String s2(L"hello world");
    s2.replace(6, String::npos, "everybody");
    REQUIRE(s2 == L"hello everybody");
}

TEST_CASE("String replaceFirst", "[string]") {
    String s = "hello world hello";
    REQUIRE(s.replaceFirst("hello", "ahoy"));
    REQUIRE(s == "ahoy world hello");
    REQUIRE_FALSE(s.replaceFirst("guten tag", "test"));
    REQUIRE(s == "ahoy world hello");
}

TEST_CASE("String replaceAll", "[string]") {
    String s = "test";
    REQUIRE(s.replaceAll("1", "grr") == 0);
    REQUIRE(s == "test");
    s = "test 1 of 1 replace 1 all";
    REQUIRE(s.replaceAll("1", "2") == 3);
    REQUIRE(s == "test 2 of 2 replace 2 all");
    REQUIRE(s.replaceAll("2", "dummy") == 3);
    REQUIRE(s == "test dummy of dummy replace dummy all");

    s = "test 1 of 1 replace 1 all";
    s.replaceAll("1", "111");
    REQUIRE(s == "test 111 of 111 replace 111 all");
    s.replaceAll("111", "1");
    REQUIRE(s == "test 1 of 1 replace 1 all");
}

TEST_CASE("String substr", "[string]") {
    String s1(L"hello world");
    REQUIRE(s1.substr(0, 5) == L"hello");
    REQUIRE(s1.substr(6) == L"world");
    REQUIRE(s1.substr(2, 888) == L"llo world");
    REQUIRE(s1.substr(3, 0) == L"");
    REQUIRE(s1.substr(s1.size()) == L"");
    REQUIRE_SPH_ASSERT(s1.substr(888));
}

TEST_CASE("String trim", "[string]") {
    String s1(L"    something");
    REQUIRE(s1.trim() == L"something");
    String s2(L"something else      ");
    REQUIRE(s2.trim() == L"something else");
    String s3(L"                   test               ");
    REQUIRE(s3.trim() == L"test");
    REQUIRE(String(L"").trim() == L"");
    REQUIRE(String(L"   ").trim() == L"");

    String s4(L"\n  \t hello \t world \t \n ");
    REQUIRE(s4.trim(String::TrimFlag::SPACE) == L"\n  \t hello \t world \t \n");
    REQUIRE(s4.trim(String::TrimFlag::END_LINE) == L"  \t hello \t world \t \n ");
    REQUIRE(s4.trim(String::TrimFlag::SPACE | String::TrimFlag::TAB | String::TrimFlag::END_LINE) ==
            L"hello \t world");
}

TEST_CASE("String lower", "[string]") {
    REQUIRE(String(L"Hello World 123").toLowercase() == L"hello world 123");
    REQUIRE(String(L"Kindly Please Convert THIS to LowerCase, thank YOU.").toLowercase() ==
            L"kindly please convert this to lowercase, thank you.");
}

TEST_CASE("String stream", "[string]") {
    std::wstringstream ss;
    ss << String(L"hello") << String(L" ") << String(L"world");
    REQUIRE(ss.str() == L"hello world");

    String s1, s2;
    ss >> s1 >> s2;
    REQUIRE(s1 == "hello");
    REQUIRE(s2 == "world");
}

TEST_CASE("String concat", "[string]") {
    String s = String(L"hello") + L" " + L"world" + L"";
    REQUIRE(s == L"hello world");
}

TEST_CASE("String compare", "[string]") {
    REQUIRE(String(L"abc") < String(L"abd"));
    REQUIRE_FALSE(String(L"abc") < String(L"abc"));
    REQUIRE_FALSE(String(L"abc") < String(L"abb"));
    REQUIRE_FALSE(String(L"abc") < String(L"aac"));
    REQUIRE(String(L"abc") < String(L"abce"));
}

TEST_CASE("String literal", "[string]") {
    auto s = L"test"_s;
    static_assert(std::is_same<decltype(s), String>::value, "static test failed");
    REQUIRE(s == L"test");
}

TEST_CASE("toString", "[string]") {
    REQUIRE(toString(5) == L"5");
    REQUIRE(toString(5.14f) == L"5.14");
    REQUIRE(toString("test") == L"test");
    REQUIRE(toString('c') == L"c");
}

TEST_CASE("String format", "[string]") {
    REQUIRE(format("") == "");
    REQUIRE(format("test") == "test");
    REQUIRE(format("hello {}", String("world")) == "hello world");
    REQUIRE(format("a {} b {} {}", 3, 4.5f, 6) == "a 3 b 4.5 6");

    REQUIRE_THROWS_AS(format("not {} enough {} params", 5), FormatException);
    REQUIRE_THROWS_AS(format("too {} many {} params", 5, 3, 1), FormatException);
}

TEST_CASE("String split", "[string]") {
    String csv = "value1,value2,value3,";
    Array<String> parts = split(csv, ',');
    REQUIRE(parts.size() == 4);
    REQUIRE(parts[0] == "value1");
    REQUIRE(parts[1] == "value2");
    REQUIRE(parts[2] == "value3");
    REQUIRE(parts[3] == "");

    parts = split(csv, '/');
    REQUIRE(parts.size() == 1);
    REQUIRE(parts[0] == csv);
}

TEST_CASE("fromString", "[string]") {
    Optional<int> i = fromString<int>("53 ");
    REQUIRE(i.value() == 53);

    Optional<float> f = fromString<float>("42.4\n");
    REQUIRE(f.value() == 42.4f);

    Optional<bool> b = fromString<bool>(" 0");
    REQUIRE_FALSE(b.value());

    Optional<Size> u = fromString<Size>(" 059 \n");
    REQUIRE(u.value() == 59);

    REQUIRE_FALSE(fromString<int>(""));
    REQUIRE_FALSE(fromString<int>("test"));
    REQUIRE_FALSE(fromString<int>("5.14"));
}

TEST_CASE("String lineBreak", "[string]") {
    REQUIRE(setLineBreak("test test", 6) == "test\ntest");
    REQUIRE(setLineBreak("test, test", 10) == "test, test");
    REQUIRE(setLineBreak("test, test", 4) == "test,\ntest");
    REQUIRE(setLineBreak("test, test", 5) == "test,\ntest");
    REQUIRE(setLineBreak("test, test", 6) == "test,\ntest");

    REQUIRE(
        setLineBreak("- option1: test test test test", 22) == "- option1: test test\n           test test");

    REQUIRE(setLineBreak("verylongwordthatcannotbesplit", 10) == "verylongwordthatcannotbesplit");
    REQUIRE(setLineBreak("verylongwordthatcannotbesplit and anotherverylongword", 6) ==
            "verylongwordthatcannotbesplit\nand\nanotherverylongword");
}

TEST_CASE("String capitalize", "[string]") {
    REQUIRE(capitalize("test") == "Test");
    REQUIRE(capitalize("  test ") == "  Test ");
    REQUIRE(capitalize("TEST") == "TEST");
    REQUIRE(capitalize("apples and oranges") == "Apples and Oranges");
    REQUIRE(capitalize("red, green or blue") == "Red, Green or Blue");
    REQUIRE(capitalize("sevecek et al") == "Sevecek et al");
    REQUIRE(capitalize("symbols!?") == "Symbols!?");
}

TEST_CASE("String UTF-8", "[string]") {
    String alpha(L"\u03B1");
    CharString utf8 = alpha.toUtf8();
    REQUIRE(utf8.size() == 2);
    REQUIRE(uint8_t(utf8[0]) == 0xce);
    REQUIRE(uint8_t(utf8[1]) == 0xb1);

    String s = String::fromUtf8(utf8);
    REQUIRE(s == alpha);
}
