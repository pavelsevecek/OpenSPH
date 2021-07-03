#include "objects/wrappers/ExtendedEnum.h"
#include "catch.hpp"
#include "objects/Exceptions.h"
#include "utils/Utils.h"

using namespace Sph;

namespace Sph {
enum class Food {
    EGG,
    BACON,
};
enum class Drink {
    MILK,
};
SPH_EXTEND_ENUM(Drink, Food);
} // namespace Sph

enum class Unrelated {
    VALUE,
};

static_assert(std::is_convertible<Food, ExtendedEnum<Food>>::value, "ExtendedEnum static test failed");
static_assert(std::is_convertible<Drink, ExtendedEnum<Food>>::value, "ExtendedEnum static test failed");
static_assert(!std::is_convertible<Unrelated, ExtendedEnum<Food>>::value, "ExtendedEnum static test failed");

TEST_CASE("Extended enum in switch", "[enum]") {
    ExtendedEnum<Food> value = Drink::MILK;

    auto checkValue = [&] {
        switch (value) {
        case Food::EGG:
            throw Exception("");
        case Food::BACON:
            throw Exception("");
        default:
            switch (Drink(value)) {
            case Drink::MILK:
                break;
            default:
                throw Exception("");
            }
        }
    };
    REQUIRE_NOTHROW(checkValue());
}
