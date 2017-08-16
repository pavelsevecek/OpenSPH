#include "io/Output.h"
#include "catch.hpp"
#include "io/Column.h"
#include "io/FileSystem.h"
#include "objects/containers/PerElementWrapper.h"
#include "objects/geometry/Domain.h"
#include "physics/Eos.h"
#include "quantities/Iterate.h"
#include "sph/Material.h"
#include "sph/initial/Initial.h"
#include "tests/Setup.h"
#include <fstream>

using namespace Sph;

static void addColumns(TextOutput& output) {
    output.add(makeAuto<ValueColumn<Float>>(QuantityId::DENSITY));
    output.add(makeAuto<ValueColumn<Vector>>(QuantityId::POSITIONS));
    output.add(makeAuto<DerivativeColumn<Vector>>(QuantityId::POSITIONS));
}

TEST_CASE("TextOutput dump", "[output]") {
    Storage storage;
    storage.insert<Vector>(
        QuantityId::POSITIONS, OrderEnum::SECOND, makeArray(Vector(0._f), Vector(1._f), Vector(2._f)));
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 5._f);
    TextOutput output(Path("tmp1_%d.txt"), "Output", EMPTY_FLAGS);
    addColumns(output);
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
    std::string content = readFile(Path("tmp1_0000.txt"));
    REQUIRE(content == expected);
}

template <typename TValue>
static bool almostEqual(const Array<TValue>& b1, const Array<TValue>& b2) {
    struct Element {
        TValue v1, v2;
    };
    for (Element e : iterateTuple<Element>(b1, b2)) {
        if (!almostEqual(e.v1, e.v2)) {
            return false;
        }
    }
    return true;
}

TEST_CASE("TextOutput dump&accumulate", "[output]") {
    TextOutput output(Path("tmp2_%d.txt"), "Output", EMPTY_FLAGS);
    Storage storage = Tests::getGassStorage(1000);
    Statistics stats;
    stats.set(StatisticsId::TOTAL_TIME, 0._f);
    addColumns(output);
    output.dump(storage, stats);

    Storage loaded;
    output.load(Path("tmp2_0000.txt"), loaded);
    REQUIRE(loaded.getQuantityCnt() == 3); // density + position + flags

    Quantity& positions = loaded.getQuantity(QuantityId::POSITIONS);
    REQUIRE(positions.getOrderEnum() == OrderEnum::FIRST); // we didn't dump accelerations
    REQUIRE(positions.getValueEnum() == ValueEnum::VECTOR);
    REQUIRE(almostEqual(positions.getValue<Vector>(), storage.getValue<Vector>(QuantityId::POSITIONS)));
    REQUIRE(almostEqual(positions.getDt<Vector>(), storage.getDt<Vector>(QuantityId::POSITIONS)));

    Quantity& density = loaded.getQuantity(QuantityId::DENSITY);
    REQUIRE(density.getOrderEnum() == OrderEnum::ZERO);
    REQUIRE(density.getValueEnum() == ValueEnum::SCALAR);
    REQUIRE(almostEqual(density.getValue<Float>(), storage.getValue<Float>(QuantityId::DENSITY)));
}

TEST_CASE("BinaryOutput dump&accumulate simple", "[output]") {
    Storage storage1;
    Array<Vector> r{ Vector(0._f), Vector(1._f), Vector(2._f) };
    Array<Vector> v{ Vector(-1._f), Vector(-2._f), Vector(-3._f) };
    storage1.insert<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND, r.clone());
    storage1.getDt<Vector>(QuantityId::POSITIONS) = v.clone();
    storage1.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 5._f);
    storage1.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::ZERO, TracelessTensor(3._f));
    storage1.insert<SymmetricTensor>(
        QuantityId::ANGULAR_MOMENTUM_CORRECTION, OrderEnum::ZERO, SymmetricTensor(6._f));
    BinaryOutput output(Path("simple%d.out"));
    Statistics stats;
    stats.set(StatisticsId::TOTAL_TIME, 0._f);
    output.dump(storage1, stats);

    Storage storage2;
    REQUIRE(output.load(Path("simple0000.out"), storage2));
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

TEST_CASE("BinaryOutput dump&accumulate materials", "[output]") {
    Storage storage;
    RunSettings settings;
    /*settings.set(RunSettingsId::MODEL_FORCE_PRESSURE_GRADIENT, false);
    settings.set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, false);*/
    InitialConditions conds(storage, settings);
    BodySettings body;
    // for exact number of particles
    body.set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::RANDOM);
    body.set(BodySettingsId::PARTICLE_COUNT, 10);
    body.set(BodySettingsId::EOS, EosEnum::TILLOTSON);
    body.set(BodySettingsId::DENSITY_RANGE, Range(4._f, 6._f));
    body.set(BodySettingsId::DENSITY_MIN, 3._f);
    conds.addBody(SphericalDomain(Vector(0._f), 2._f), body);

    body.set(BodySettingsId::PARTICLE_COUNT, 20);
    body.set(BodySettingsId::EOS, EosEnum::IDEAL_GAS);
    body.set(BodySettingsId::DENSITY_RANGE, Range(1._f, 2._f));
    body.set(BodySettingsId::DENSITY_MIN, 5._f);
    conds.addBody(SphericalDomain(Vector(0._f), 1._f), body);

    body.set(BodySettingsId::PARTICLE_COUNT, 5);
    body.set(BodySettingsId::EOS, EosEnum::MURNAGHAN);
    body.set(BodySettingsId::DENSITY, 100._f);
    conds.addBody(SphericalDomain(Vector(0._f), 0.5_f), body);

    /*body.set(BodySettingsId::PARTICLE_COUNT, 15);
    body.set(BodySettingsId::EOS, EosEnum::NONE);
    body.set(BodySettingsId::RHEOLOGY_DAMAGE, DamageEnum::NONE);
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE);
    conds.addBody(SphericalDomain(Vector(0._f), 1.5_f), body);*/

    BinaryOutput output(Path("mat%d.out"));
    Statistics stats;
    /// \todo accumulate stats
    stats.set(StatisticsId::TOTAL_TIME, 0._f);
    output.dump(storage, stats);

    // sanity check
    REQUIRE(storage.getMaterialCnt() == 3);
    REQUIRE(storage.getParticleCnt() == 35);
    REQUIRE(storage.getQuantityCnt() == 12);

    Storage loaded;
    REQUIRE(output.load(Path("mat0000.out"), loaded));
    REQUIRE(loaded.getMaterialCnt() == storage.getMaterialCnt());
    REQUIRE(loaded.getParticleCnt() == storage.getParticleCnt());
    REQUIRE(loaded.getQuantityCnt() == storage.getQuantityCnt());

    // absolute match of two storages
    iteratePair<VisitorEnum::ALL_BUFFERS>(loaded, storage, [](auto& b1, auto& b2) { REQUIRE(b1 == b2); });

    MaterialView mat = loaded.getMaterial(0);
    REQUIRE(mat->range(QuantityId::DENSITY) == Range(4._f, 6._f));
    REQUIRE(mat->minimal(QuantityId::DENSITY) == 3._f);
    REQUIRE(mat.sequence() == IndexSequence(0, 10));
    EosMaterial* eosMat = dynamic_cast<EosMaterial*>(&mat.material());
    REQUIRE(dynamic_cast<const TillotsonEos*>(&eosMat->getEos()));

    mat = loaded.getMaterial(1);
    REQUIRE(mat->range(QuantityId::DENSITY) == Range(1._f, 2._f));
    REQUIRE(mat->minimal(QuantityId::DENSITY) == 5._f);
    REQUIRE(mat.sequence() == IndexSequence(10, 30));
    eosMat = dynamic_cast<EosMaterial*>(&mat.material());
    REQUIRE(dynamic_cast<const IdealGasEos*>(&eosMat->getEos()));

    mat = loaded.getMaterial(2);
    REQUIRE(mat->getParam<Float>(BodySettingsId::DENSITY) == 100._f);
    REQUIRE(mat.sequence() == IndexSequence(30, 35));
    eosMat = dynamic_cast<EosMaterial*>(&mat.material());
    REQUIRE(dynamic_cast<const MurnaghanEos*>(&eosMat->getEos()));
}

TEST_CASE("Pkdgrav output", "[output]") {
    BodySettings settings;
    settings.set(BodySettingsId::ENERGY, 50._f);
    Storage storage = Tests::getGassStorage(100, settings);
    storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, 0);

    PkdgravOutput output(Path("readerik%d.out"), PkdgravParams{});
    Statistics stats;
    output.dump(storage, stats);
    REQUIRE(fileSize(Path("readerik0000.out")) > 0);

    PkdgravParams params;
    params.vaporThreshold = 0.f;
    PkdgravOutput output2(Path("readerik2_%d.out"), std::move(params));
    output2.dump(storage, stats);
    REQUIRE(fileSize(Path("readerik2_0000.out")) == 0);
}
