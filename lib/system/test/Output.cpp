#include "system/Output.h"
#include "catch.hpp"
#include <thread>

using namespace Sph;


TEST_CASE("Dumping data", "[output]") {
    Storage storage;
    storage.emplace<Vector, OrderEnum::SECOND_ORDER>(
        QuantityKey::POSITIONS, makeArray(Vector(0._f), Vector(1._f), Vector(2._f)));
    storage.emplace<Float, OrderEnum::FIRST_ORDER>(QuantityKey::DENSITY, 5._f);
    TextOutput output("tmp%d.out", "Output", { QuantityKey::DENSITY, QuantityKey::POSITIONS });
    output.dump(storage, 0);

    std::string expected = R"(# Run: Output
# SPH dump, time = 0
#         Density   Position [x]   Position [y]   Position [z]   Velocity [x]   Velocity [y]   Velocity [z]
              5              0              0              0              0              0              0
              5              1              1              1              0              0              0
              5              2              2              2              0              0              0
)";
    std::ifstream file("tmp0000.out");
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == expected);
}
