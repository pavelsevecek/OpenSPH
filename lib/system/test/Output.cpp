#include "system/Output.h"
#include "catch.hpp"
#include <thread>

using namespace Sph;


TEST_CASE("Dumping data", "[output]") {
    Storage storage;
    storage.insert<QuantityKey::R>();
    storage.get<QuantityKey::R>().push(Vector(1._f, 2._f, 3._f));
    storage.dt<QuantityKey::R>().push(Vector(4._f, 5._f, 6._f));
    TextOutput output("tmp%d.out");
    output.dump(storage, 0);
}
