#include "quantities/Particle.h"
#include "catch.hpp"
#include "tests/Setup.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Particle from storage", "[particle]") {
    Storage storage = Tests::getGassStorage(100);
    storage.getValue<Float>(QuantityId::MASS)[4] = 5._f;
    storage.getValue<Vector>(QuantityId::POSITION)[4] = Vector(3._f, 2._f, 1._f);
    storage.getDt<Vector>(QuantityId::POSITION)[4] = Vector(1._f, 0._f, 0._f);

    Particle p(storage, 4);
    REQUIRE(p.getIndex() == 4);
    REQUIRE(p.getValue(QuantityId::MASS) == 5._f);
    REQUIRE(p.getValue(QuantityId::POSITION) == Vector(3._f, 2._f, 1._f));
    REQUIRE(p.getDt(QuantityId::POSITION) == Vector(1._f, 0._f, 0._f));

    REQUIRE_ASSERT(p.getValue(QuantityId::DAMAGE));
}

TEST_CASE("Particle from values", "[particle]") {
    Particle p(QuantityId::MASS, Vector(4._f), 3);
    REQUIRE(p.getIndex() == 3);
    REQUIRE(p.getValue(QuantityId::MASS) == Vector(4._f));
    REQUIRE(p.getDt(QuantityId::MASS).empty());
    REQUIRE_ASSERT(p.getValue(QuantityId::DENSITY));
}

TEST_CASE("Particle explicit", "[particle]") {
    Particle p(5);
    p.addValue(QuantityId::AV_ALPHA, 5._f);
    p.addD2t(QuantityId::DAMAGE, SymmetricTensor(3._f));

    REQUIRE(p.getIndex() == 5);
    REQUIRE(p.getValue(QuantityId::AV_ALPHA) == 5._f);
    REQUIRE(p.getD2t(QuantityId::DAMAGE) == SymmetricTensor(3._f));
    REQUIRE(p.getDt(QuantityId::DAMAGE).empty());
    REQUIRE_ASSERT(p.getValue(QuantityId::ENERGY));
}

TEST_CASE("Particle iterate", "[particle]") {
    Particle p;
    p.addValue(QuantityId::MASS, 5._f);
    p.addValue(QuantityId::DAMAGE, 3._f);
    p.addDt(QuantityId::FLAG, Vector(2._f));

    Size i = 0;
    for (Particle::QuantityData data : p) {
        REQUIRE(i < 3);
        switch (i) {
        case 0:
            REQUIRE(data.id == QuantityId::MASS);
            REQUIRE(data.type == DynamicId::FLOAT);
            REQUIRE(data.value == 5._f);
            REQUIRE(data.dt.empty());
            REQUIRE(data.d2t.empty());
            break;
        case 1:
            REQUIRE(data.id == QuantityId::DAMAGE);
            REQUIRE(data.type == DynamicId::FLOAT);
            REQUIRE(data.value == 3._f);
            REQUIRE(data.dt.empty());
            REQUIRE(data.d2t.empty());
            break;
        case 2:
            REQUIRE(data.id == QuantityId::FLAG);
            REQUIRE(data.type == DynamicId::VECTOR);
            REQUIRE(data.value.empty());
            REQUIRE(data.dt == Vector(2._f));
            REQUIRE(data.d2t.empty());
            break;
        }

        ++i;
    }
}
