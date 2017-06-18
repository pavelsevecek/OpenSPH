#include "io/Output.h"
#include "catch.hpp"
#include "io/Column.h"
#include "io/FileSystem.h"
#include "objects/containers/PerElementWrapper.h"
#include <fstream>

using namespace Sph;


TEST_CASE("TextOutput dump", "[output]") {
    Storage storage;
    storage.insert<Vector>(
        QuantityId::POSITIONS, OrderEnum::SECOND, makeArray(Vector(0._f), Vector(1._f), Vector(2._f)));
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 5._f);
    TextOutput output(Path("tmp%d.txt"), "Output", EMPTY_FLAGS);
    output.add(makeAuto<ValueColumn<Float>>(QuantityId::DENSITY));
    output.add(makeAuto<ValueColumn<Vector>>(QuantityId::POSITIONS));
    output.add(makeAuto<DerivativeColumn<Vector>>(QuantityId::POSITIONS));
    Statistics stats;
    stats.set(StatisticsId::TOTAL_TIME, 0._f);
    output.dump(storage, stats);

    std::string expected = R"(# Run: Output
# SPH dump, time = 0
#              Density        Position [x]        Position [y]        Position [z]        Velocity [x]        Velocity [y]        Velocity [z]
                   5                   0                   0                   0                   0                   0                   0
                   5                   1                   1                   1                   0                   0                   0
                   5                   2                   2                   2                   0                   0                   0
)";
    std::string content = readFile(Path("tmp0000.txt"));
    REQUIRE(content == expected);
}

TEST_CASE("BinaryOutput dump&accumulate", "[output]") {
    Storage storage1;
    Array<Vector> r{ Vector(0._f), Vector(1._f), Vector(2._f) };
    Array<Vector> v{ Vector(-1._f), Vector(-2._f), Vector(-3._f) };
    storage1.insert<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND, r.clone());
    storage1.getDt<Vector>(QuantityId::POSITIONS) = v.clone();
    storage1.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 5._f);
    storage1.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::ZERO, TracelessTensor(3._f));
    storage1.insert<SymmetricTensor>(
        QuantityId::ANGULAR_MOMENTUM_CORRECTION, OrderEnum::ZERO, SymmetricTensor(6._f));
    BinaryOutput output(Path("tmp%d.out"), "Output");
    Statistics stats;
    stats.set(StatisticsId::TOTAL_TIME, 0._f);
    output.dump(storage1, stats);

    Storage storage2;
    REQUIRE(output.load(Path("tmp0000.out"), storage2));
    REQUIRE(storage2.getParticleCnt() == 3);
    REQUIRE(storage2.getQuantityCnt() == 4);

    REQUIRE(storage2.getValue<Vector>(QuantityId::POSITIONS) == r);
    REQUIRE(storage2.getDt<Vector>(QuantityId::POSITIONS) == v);
    REQUIRE(perElement(storage2.getD2t<Vector>(QuantityId::POSITIONS)) == Vector(0._f));

    REQUIRE(storage2.getQuantity(QuantityId::DENSITY).getOrderEnum() == OrderEnum::FIRST);
    REQUIRE(perElement(storage2.getValue<Float>(QuantityId::DENSITY)) == 5._f);
    REQUIRE(storage2.getQuantity(QuantityId::DEVIATORIC_STRESS).getOrderEnum() == OrderEnum::ZERO);
    REQUIRE(perElement(storage2.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS)) ==
            TracelessTensor(3._f));
    REQUIRE(storage2.getQuantity(QuantityId::ANGULAR_MOMENTUM_CORRECTION).getOrderEnum() == OrderEnum::ZERO);
    REQUIRE(perElement(storage2.getValue<SymmetricTensor>(QuantityId::ANGULAR_MOMENTUM_CORRECTION)) ==
            SymmetricTensor(6._f));
}
