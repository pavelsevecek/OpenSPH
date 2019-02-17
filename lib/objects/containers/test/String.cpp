#include "objects/containers/String.h"
#include "catch.hpp"
#include "objects/utility/IteratorAdapters.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("String default construct", "[string]") {
    String s0;
    REQUIRE(s0.size() == 0);
    REQUIRE(s0.empty());
    REQUIRE_ASSERT(s0[0]);

    REQUIRE(s0.begin() == s0.end());
}

TEST_CASE("String copy construct", "[string]") {
    String s0;
    String s1(s0);
    REQUIRE(s1.size() == 0);
    REQUIRE(s1.empty());

    String s2 = "test";
    REQUIRE(s2.size() == 4);
    REQUIRE_FALSE(s2.empty());
    REQUIRE(s2[0] == 't');
    REQUIRE(s2[1] == 'e');
    REQUIRE(s2[2] == 's');
    REQUIRE(s2[3] == 't');
    REQUIRE_ASSERT(s2[4]);

    String s3(s2);
    REQUIRE(s3.size() == 4);
    REQUIRE(s2 == s3);
}

TEST_CASE("String move construct", "[string]") {
    String s1("test");
    String s2(std::move(s1));
    REQUIRE(s2.size() == 4);
    REQUIRE(s2 == "test");
    REQUIRE(s1.empty());
    REQUIRE(s1 == "");
}

TEST_CASE("String construct from buffer", "[string]") {
    Array<char> buffer1{ 'a', 'b', 'c' };
    REQUIRE_ASSERT(String(std::move(buffer1)));

    Array<char> buffer2{ 'a', 'b', 'c', '\0' };
    String s2(std::move(buffer2));
    REQUIRE(s2.size() == 3);
    REQUIRE(s2 == "abc");
}

TEST_CASE("String assign", "[string]") {
    String s1("abc");
    s1 = "test";
    REQUIRE(s1 == "test");

    String s2("other");
    s1 = std::move(s2);
    REQUIRE(s1 == "other");
    REQUIRE(s2 == "test"); // Array move swaps content
}

TEST_CASE("String append", "[string]") {
    String s("hello");
    s += ' ';
    s += "world";
    REQUIRE(s == "hello world");

    String s2(" again");
    s += s2;
    REQUIRE(s == "hello world again");
}

TEST_CASE("String cStr", "[string]") {
    String s("test");
    const char* c = s.cStr();
    REQUIRE(strlen(c) == 4);
    REQUIRE(strcmp(c, "test") == 0);
}

TEST_CASE("String iterator", "[string]") {
    String s1("hello");
    for (char& c : s1) {
        c = 'a';
    }
    REQUIRE(s1 == "aaaaa");

    String s2("hello");
    struct Element {
        char& c1;
        char& c2;
    };
    for (auto i : iterateTuple<Element>(s1, reverse(s2))) {
        i.c1 = i.c2;
    }
    REQUIRE(s1 == "olleh");
}

TEST_CASE("String find", "[string]") {
    String s0("test");
    REQUIRE(s0.find("test") == 0);

    String s1("hello world");
    REQUIRE(s1.find("hello") == 0);
    REQUIRE(s1.find("world") == 6);
    REQUIRE(s1.find("o") == 4);
    REQUIRE(s1.find("o", 5) == 7);
    REQUIRE(s1.find("hello", 1) == String::npos);
    REQUIRE(s1.find("hello world2") == String::npos);
    REQUIRE(s1.find("test") == String::npos);
    REQUIRE(s1.find("rlda") == String::npos);

    String s2("aaab");
    REQUIRE(s2.find("aaab") == 0);
    REQUIRE(s2.find("aab") == 1);
    REQUIRE(s2.find("b") == 3);
    REQUIRE(s2.find("a") == 0);
    REQUIRE(s2.find("a", 2) == 2);

    REQUIRE_ASSERT(s2.find(""));
    REQUIRE_ASSERT(s2.find("a", 4));
}

TEST_CASE("String find last", "[string]") {
    String s0("test");
    REQUIRE(s0.findLast("test") == 0);

    String s1("abc abc");
    REQUIRE(s1.findLast("abc") == 4);
    REQUIRE(s1.findLast("def") == String::npos);
    REQUIRE(s1.findLast("abc abc abc") == String::npos);
    REQUIRE_ASSERT(s1.findLast(""));

    String s2("hello world");
    REQUIRE(s2.findLast("l") == 9);
}

TEST_CASE("String replace", "[string]") {
    String s1("hello world");
    s1.replace(0, 2, "a");
    REQUIRE(s1 == "allo world");
    REQUIRE_ASSERT(s1.replace(0, 1000, "test"));
    s1.replace(0, s1.size(), "test");
    REQUIRE(s1 == "test");
    s1.replace(0, 1, "the larg");
    REQUIRE(s1 == "the largest");

    String s2("String %e with %d some %i placeholders");
    s2.replace("%e", "test", "%d", "something", "%i", "888");
    REQUIRE(s2 == "String test with something some 888 placeholders");
}

TEST_CASE("String substr", "[string]") {
    String s1("hello world");
    REQUIRE(s1.substr(0, 5) == "hello");
    REQUIRE(s1.substr(6) == "world");
    REQUIRE(s1.substr(2, 888) == "llo world");
    REQUIRE_ASSERT(s1.substr(888));
}

TEST_CASE("String trim", "[string]") {
    String s1("    something");
    REQUIRE(s1.trim() == "something");
    String s2("something else      ");
    REQUIRE(s2.trim() == "something else");
    String s3("                   test               ");
    REQUIRE(s3.trim() == "test");
    REQUIRE(String("").trim() == "");
    REQUIRE(String("   ").trim() == "");
}

TEST_CASE("String lower", "[string]") {
    REQUIRE(String("Hello World 123").lower() == "hello world 123");
    REQUIRE(String("Kindly Please Convert THIS to LowerCase, thank YOU.").lower() ==
            "kindly please convert this to lowercase, thank you.");
}

TEST_CASE("String stream", "[string]") {
    std::stringstream ss;
    ss << String("hello") << String(" ") << String("world");
    REQUIRE(ss.str() == "hello world");

    /*String s1, s2;
    ss >> s1 >> s2;
    REQUIRE(s1 == "hello");
    REQUIRE(s2 == "world");*/
}

TEST_CASE("String concat", "[string]") {
    String s = String("hello") + " " + "world" + "";
    REQUIRE(s == "hello world");
}

TEST_CASE("String compare", "[string]") {
    REQUIRE(String("abc") < String("abd"));
    REQUIRE_FALSE(String("abc") < String("abc"));
    REQUIRE_FALSE(String("abc") < String("abb"));
    REQUIRE_FALSE(String("abc") < String("aac"));
    REQUIRE(String("abc") < String("abce"));
}

TEST_CASE("String literal", "[string]") {
    auto s = "test"_s;
    static_assert(std::is_same<decltype(s), String>::value, "static test failed");
    REQUIRE(s == "test");
}

TEST_CASE("toString", "[string]") {
    REQUIRE(toString(5) == "5");
    REQUIRE(toString(5.14f) == "5.14");
    REQUIRE(toString("test") == "test");
    REQUIRE(toString('c') == "c");
}
