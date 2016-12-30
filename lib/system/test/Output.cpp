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
}
