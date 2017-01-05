#include "objects/wrappers/Variant.h"
#include "catch.hpp"
#include "utils/RecordType.h"

using namespace Sph;


TEST_CASE("Variant constructor", "[variant]") {
    RecordType::resetStats();
    Variant<int, float, RecordType> variant1;
    REQUIRE(variant1.getTypeIdx() == -1);
    REQUIRE(!variant1.tryGet<int>());
    REQUIRE(!variant1.tryGet<float>());
    REQUIRE(!variant1.tryGet<RecordType>());
    REQUIRE(RecordType::constructedNum == 0);

    Variant<int, float> variant2(3.14f);
    REQUIRE(variant2.getTypeIdx() == 1);
    REQUIRE((float)variant2 == 3.14f);
    REQUIRE(!variant2.tryGet<int>());

    Variant<RecordType> variant3(RecordType(5));
    RecordType& r1 = variant3;
    REQUIRE(r1.wasMoveConstructed);
    REQUIRE(r1.value == 5);

    RecordType r2(3);
    Variant<RecordType> variant4(r2);
    RecordType& r3 = variant4;
    REQUIRE(r3.wasCopyConstructed);
    REQUIRE(r3.value == 3);
}

TEST_CASE("Variant copy construct", "[variant]") {
    Variant<RecordType, float> variant1(RecordType(5));
    Variant<RecordType, float> variant2(variant1);
    REQUIRE(variant2.getTypeIdx() == 0);
    RecordType& r = variant2;
    REQUIRE(r.wasCopyConstructed);
    REQUIRE(r.value == 5);
}

TEST_CASE("Variant move construct", "[variant]") {
    Variant<RecordType, float> variant1(RecordType(5));
    RecordType& r1 = variant1;
    Variant<RecordType, float> variant2(std::move(variant1));
    REQUIRE(variant2.getTypeIdx() == 0);
    RecordType& r2 = variant2;
    REQUIRE(r2.wasMoveConstructed);
    REQUIRE(r2.value == 5);
    REQUIRE(r1.wasMoved);
    REQUIRE(variant1.getTypeIdx() == -1);
}

TEST_CASE("Variant assignemt", "[variant]") {
    Variant<int, RecordType> variant1(1);
    variant1 = RecordType(5);
    REQUIRE(variant1.getTypeIdx() == 1);
    RecordType& r1 = variant1;
    REQUIRE(r1.wasMoveConstructed);
    REQUIRE(r1.value == 5);

    RecordType::resetStats();
    RecordType rhs(7);
    variant1 = rhs;
    REQUIRE(RecordType::destructedNum == 0);
    REQUIRE(variant1.getTypeIdx() == 1);
    RecordType& r2 = variant1;
    REQUIRE(r2.wasCopyAssigned);
    REQUIRE(r2.value == 7);

    variant1 = 3;
    REQUIRE(RecordType::destructedNum == 1);
    REQUIRE(variant1.getTypeIdx() == 0);
    REQUIRE((int)variant1 == 3);

    Variant<int, RecordType> variant2(3);
    RecordType r3(6);
    variant2 = r3;
    REQUIRE(variant2.getTypeIdx() == 1);
    RecordType& r4 = variant2;
    REQUIRE(r4.wasCopyConstructed);
    REQUIRE(r4.value == 6);
}

TEST_CASE("Variant copy", "[variant]") {
    Variant<int, RecordType> variant1(RecordType(5));
    Variant<int, RecordType> variant2;
    variant2 = variant1;
    REQUIRE(variant2.getTypeIdx() == 1);
    RecordType& r1 = variant2;
    REQUIRE(r1.wasCopyConstructed);
    REQUIRE(r1.value == 5);

    Variant<int, RecordType> variant3(10);
    variant3 = variant1;
    REQUIRE(variant3.getTypeIdx() == 1);
    RecordType& r2 = variant3;
    REQUIRE(r2.wasCopyConstructed);
    REQUIRE(r2.value == 5);

    Variant<int, RecordType> variant4(RecordType(1));
    variant4 = variant1;
    RecordType& r3 = variant4;
    REQUIRE(r3.wasMoveConstructed);
    REQUIRE(r3.wasCopyAssigned);
    REQUIRE(r3.value == 5);

    Variant<int, RecordType> variant5(8);
    RecordType::resetStats();
    variant4 = variant5;
    REQUIRE(variant4.getTypeIdx() == 0);
    REQUIRE(RecordType::destructedNum == 1);
    REQUIRE((int)variant4 == 8);
}

TEST_CASE("Variant move", "[variant]") {
    Variant<int, RecordType> variant1(RecordType(5));
    RecordType& r1 = variant1;
    Variant<int, RecordType> variant2;
    variant2 = std::move(variant1);
    REQUIRE(variant2.getTypeIdx() == 1);
    RecordType& r2 = variant2;
    REQUIRE(r2.wasMoveConstructed);
    REQUIRE(r2.value == 5);
    REQUIRE(r1.wasMoved);

    Variant<int, RecordType> variant3(RecordType(6));
    Variant<int, RecordType> variant4(10);
    variant4 = std::move(variant3);
    REQUIRE(variant4.getTypeIdx() == 1);
    RecordType& r3 = variant4;
    REQUIRE(r3.wasMoveConstructed);
    REQUIRE(r3.value == 6);

    Variant<int, RecordType> variant5(RecordType(9));
    RecordType& r4 = variant5;
    Variant<int, RecordType> variant6(RecordType(8));
    variant6 = std::move(variant5);
    RecordType& r5 = variant6;
    REQUIRE(r5.wasMoveConstructed);
    REQUIRE(r5.wasMoveAssigned);
    REQUIRE(r5.value == 9);
    REQUIRE(r4.wasMoved);
}

TEST_CASE("Variant get", "[variant]") {
    Variant<int, float> variant1;
    REQUIRE(!variant1.tryGet<int>());
    REQUIRE(!variant1.tryGet<float>());
    variant1 = 20;
    REQUIRE(variant1.get<int>() == 20);
    REQUIRE(!variant1.tryGet<float>());
    variant1 = 3.14f;
    REQUIRE(!variant1.tryGet<int>());
    REQUIRE(variant1.get<float>() == 3.14f);
}

TEST_CASE("Variant empty string", "[variant]") {
    Variant<int, std::string> variant1;
    variant1 = std::string("");
    REQUIRE(variant1.tryGet<std::string>());
    REQUIRE(variant1.get<std::string>() == std::string(""));
}

TEST_CASE("Variant forValue", "[variant]") {
    Variant<int, float, double, char> variant(5.f);
    forValue(variant, [](auto&& value) {
        // this cannot be static assert as the function is instantiated for all types
        REQUIRE((std::is_same<std::decay_t<decltype(value)>, float>::value));
        REQUIRE(value == 5.f);
    });
    variant = 'c';
    forValue(variant, [](auto&& value) {
        REQUIRE((std::is_same<std::decay_t<decltype(value)>, char>::value));
        REQUIRE(value == 'c');
        value = 'd';
    });
    REQUIRE(variant.get<char>() == 'd');
    struct Dispatcher {
        int operator()(const char) {
            return 1;
        }
        int operator()(const int) {
            return 2;
        }
        int operator()(const float) {
            return 3;
        }
        int operator()(const double){
            return 4;
        }
    } dispatcher;
    REQUIRE(forValue(variant, dispatcher) == 1);
    variant = 9.;
    REQUIRE(forValue(variant, dispatcher) == 4);
    variant = 3;
    REQUIRE(forValue(variant, dispatcher) == 2);
}
