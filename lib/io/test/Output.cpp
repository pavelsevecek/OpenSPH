#include "io/Output.h"
#include "catch.hpp"
#include "io/Column.h"
#include "io/FileManager.h"
#include "io/FileSystem.h"
#include "objects/geometry/Domain.h"
#include "objects/utility/PerElementWrapper.h"
#include "physics/Eos.h"
#include "quantities/Iterate.h"
#include "sph/Materials.h"
#include "sph/initial/Initial.h"
#include "system/Factory.h"
#include "tests/Approx.h"
#include "tests/Setup.h"
#include "thread/Pool.h"
#include "utils/Config.h"
#include "utils/Utils.h"
#include <fstream>

using namespace Sph;

template <typename TIo>
static void addColumns(TIo& io) {
    io.addColumn(makeAuto<ValueColumn<Float>>(QuantityId::DENSITY));
    io.addColumn(makeAuto<ValueColumn<Vector>>(QuantityId::POSITION));
    io.addColumn(makeAuto<DerivativeColumn<Vector>>(QuantityId::POSITION));
}

TEST_CASE("TextOutput dump", "[output]") {
    Storage storage;
    storage.insert<Vector>(
        QuantityId::POSITION, OrderEnum::SECOND, makeArray(Vector(0._f), Vector(1._f), Vector(2._f)));
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 5._f);
    TextOutput output(Path("tmp1_%d.txt"), "Output", EMPTY_FLAGS);
    addColumns(output);
    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, 0._f);
    output.dump(storage, stats);

    std::string expected = R"(# Run: Output
# SPH dump, time = 0
#              Density        Position [x]        Position [y]        Position [z]        Velocity [x]        Velocity [y]        Velocity [z]
                   5                   0                   0                   0                   0                   0                   0
                   5                   1                   1                   1                   0                   0                   0
                   5                   2                   2                   2                   0                   0                   0
)";
    std::string content = FileSystem::readFile(Path("tmp1_0000.txt"));
    REQUIRE(content == expected);

    output.dump(storage, stats);
    REQUIRE(FileSystem::pathExists(Path("tmp1_0001.txt")));
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
    RandomPathManager manager;
    Path path = manager.getPath("txt");
    TextOutput output(path, "Output", EMPTY_FLAGS);
    Storage storage = Tests::getGassStorage(1000);
    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, 0._f);
    addColumns(output);
    output.dump(storage, stats);

    Storage loaded;
    TextInput input(EMPTY_FLAGS);
    addColumns(input);
    input.load(path, loaded, stats);
    REQUIRE(loaded.getQuantityCnt() == 3); // density + position + flags

    Quantity& positions = loaded.getQuantity(QuantityId::POSITION);
    REQUIRE(positions.getOrderEnum() == OrderEnum::FIRST); // we didn't dump accelerations
    REQUIRE(positions.getValueEnum() == ValueEnum::VECTOR);
    REQUIRE(almostEqual(positions.getValue<Vector>(), storage.getValue<Vector>(QuantityId::POSITION)));
    REQUIRE(almostEqual(positions.getDt<Vector>(), storage.getDt<Vector>(QuantityId::POSITION)));

    Quantity& density = loaded.getQuantity(QuantityId::DENSITY);
    REQUIRE(density.getOrderEnum() == OrderEnum::ZERO);
    REQUIRE(density.getValueEnum() == ValueEnum::SCALAR);
    REQUIRE(almostEqual(density.getValue<Float>(), storage.getValue<Float>(QuantityId::DENSITY)));
}

TEST_CASE("TextOutput create from settings", "[output]") {
    RandomPathManager manager;
    RunSettings settings;
    Path path = manager.getPath("txt");
    settings.set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::TEXT_FILE);
    settings.set(RunSettingsId::RUN_OUTPUT_PATH, std::string(""));
    settings.set(RunSettingsId::RUN_OUTPUT_NAME, path.native());
    AutoPtr<IOutput> output = Factory::getOutput(settings);

    Storage storage = Tests::getSolidStorage(100);

    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, 0._f);
    output->dump(storage, stats);

    Storage loaded;
    TextInput input(settings.getFlags<OutputQuantityFlag>(RunSettingsId::RUN_OUTPUT_QUANTITIES));
    REQUIRE(input.load(path, loaded, stats));
    REQUIRE(loaded.getParticleCnt() == storage.getParticleCnt());
    // check that the basic quantities are saved
    REQUIRE(loaded.has(QuantityId::POSITION));
    REQUIRE(loaded.has(QuantityId::DENSITY));
    REQUIRE(loaded.has(QuantityId::PRESSURE));
    REQUIRE(loaded.has(QuantityId::ENERGY));
    REQUIRE(loaded.has(QuantityId::DEVIATORIC_STRESS));
}

TEST_CASE("BinaryOutput dump&accumulate simple", "[output]") {
    Storage storage1;
    Array<Vector> r{ Vector(0._f), Vector(1._f), Vector(2._f) };
    Array<Vector> v{ Vector(-1._f), Vector(-2._f), Vector(-3._f) };
    storage1.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, r.clone());
    storage1.getDt<Vector>(QuantityId::POSITION) = v.clone();
    storage1.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 5._f);
    storage1.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::ZERO, TracelessTensor(3._f));
    storage1.insert<SymmetricTensor>(
        QuantityId::STRAIN_RATE_CORRECTION_TENSOR, OrderEnum::ZERO, SymmetricTensor(6._f));

    RandomPathManager manager;
    Path path = manager.getPath("out");
    BinaryOutput output(path);
    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, 0._f);
    stats.set(StatisticsId::TIMESTEP_VALUE, 0._f);
    output.dump(storage1, stats);

    Storage storage2;
    BinaryInput input;
    REQUIRE(input.load(path, storage2, stats));
    REQUIRE(storage2.getParticleCnt() == storage1.getParticleCnt());
    REQUIRE(storage2.getQuantityCnt() == storage2.getQuantityCnt());

    REQUIRE(storage2.getValue<Vector>(QuantityId::POSITION) == r);
    REQUIRE(storage2.getDt<Vector>(QuantityId::POSITION) == v);
    REQUIRE(perElement(storage2.getD2t<Vector>(QuantityId::POSITION)) == Vector(0._f));

    REQUIRE(storage2.getQuantity(QuantityId::DENSITY).getOrderEnum() == OrderEnum::FIRST);
    REQUIRE(perElement(storage2.getValue<Float>(QuantityId::DENSITY)) == 5._f);
    REQUIRE(storage2.getQuantity(QuantityId::DEVIATORIC_STRESS).getOrderEnum() == OrderEnum::ZERO);
    REQUIRE(perElement(storage2.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS)) ==
            TracelessTensor(3._f));
    REQUIRE(
        storage2.getQuantity(QuantityId::STRAIN_RATE_CORRECTION_TENSOR).getOrderEnum() == OrderEnum::ZERO);
    REQUIRE(perElement(storage2.getValue<SymmetricTensor>(QuantityId::STRAIN_RATE_CORRECTION_TENSOR)) ==
            SymmetricTensor(6._f));
}

TEST_CASE("BinaryOutput dump&accumulate materials", "[output]") {
    Storage storage;
    RunSettings settings;
    settings.set(RunSettingsId::SPH_DISCRETIZATION, DiscretizationEnum::BENZ_ASPHAUG);
    /*settings.set(RunSettingsId::MODEL_FORCE_PRESSURE_GRADIENT, false);
    settings.set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, false);*/
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    InitialConditions conds(pool, settings);
    BodySettings body;
    // for exact number of particles
    body.set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::RANDOM);
    body.set(BodySettingsId::PARTICLE_COUNT, 10);
    body.set(BodySettingsId::EOS, EosEnum::TILLOTSON);
    body.set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE);
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum ::ELASTIC);
    body.set(BodySettingsId::DENSITY_RANGE, Interval(4._f, 6._f));
    body.set(BodySettingsId::DENSITY_MIN, 3._f);
    conds.addMonolithicBody(storage, SphericalDomain(Vector(0._f), 2._f), body);

    body.set(BodySettingsId::PARTICLE_COUNT, 20);
    body.set(BodySettingsId::EOS, EosEnum::IDEAL_GAS);
    body.set(BodySettingsId::DENSITY_RANGE, Interval(1._f, 2._f));
    body.set(BodySettingsId::DENSITY_MIN, 5._f);
    conds.addMonolithicBody(storage, SphericalDomain(Vector(0._f), 1._f), body);

    body.set(BodySettingsId::PARTICLE_COUNT, 5);
    body.set(BodySettingsId::EOS, EosEnum::MURNAGHAN);
    body.set(BodySettingsId::DENSITY, 100._f);
    conds.addMonolithicBody(storage, SphericalDomain(Vector(0._f), 0.5_f), body);

    RandomPathManager manager;
    Path path = manager.getPath("out");
    BinaryOutput output(path);
    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, 0._f);
    stats.set(StatisticsId::TIMESTEP_VALUE, 0._f);
    output.dump(storage, stats);

    // sanity check
    REQUIRE(storage.getMaterialCnt() == 3);
    REQUIRE(storage.getParticleCnt() == 35);
    // positions, masses, pressure, density, energy, sound speed, deviatoric stress, yielding reduction,
    // velocity divergence, velocity gradient, neighbour count, flags, material count
    REQUIRE(storage.getQuantityCnt() == 13);

    BinaryInput input;
    Expected<BinaryInput::Info> info = input.getInfo(path);
    REQUIRE(info);
    REQUIRE(info->materialCnt == 3);
    REQUIRE(info->particleCnt == 35);
    REQUIRE(info->quantityCnt == 12); // matIds not stored

    Storage loaded;
    REQUIRE(input.load(path, loaded, stats));
    REQUIRE(loaded.getMaterialCnt() == storage.getMaterialCnt());
    REQUIRE(loaded.getParticleCnt() == storage.getParticleCnt());
    REQUIRE(loaded.getQuantityCnt() == storage.getQuantityCnt());

    // absolute match of two storages
    iteratePair<VisitorEnum::ALL_BUFFERS>(loaded, storage, [](auto& b1, auto& b2) { REQUIRE(b1 == b2); });

    MaterialView mat = loaded.getMaterial(0);
    REQUIRE(mat->range(QuantityId::DENSITY) == Interval(4._f, 6._f));
    REQUIRE(mat->minimal(QuantityId::DENSITY) == 3._f);
    REQUIRE(mat.sequence() == IndexSequence(0, 10));
    EosMaterial* eosMat = dynamic_cast<EosMaterial*>(&mat.material());
    REQUIRE(dynamic_cast<const TillotsonEos*>(&eosMat->getEos()));

    mat = loaded.getMaterial(1);
    REQUIRE(mat->range(QuantityId::DENSITY) == Interval(1._f, 2._f));
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

TEST_CASE("BinaryOutput dump stats", "[output]") {
    Storage storage = Tests::getGassStorage(10);
    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, 24._f);
    stats.set(StatisticsId::TIMESTEP_VALUE, 0.1_f);
    RandomPathManager manager;
    Path path = manager.getPath("out");
    BinaryOutput output(path, RunTypeEnum::RUBBLE_PILE);
    output.dump(storage, stats);

    storage.removeAll();
    Statistics loadedStats;
    BinaryInput input;
    REQUIRE(input.load(path, storage, loadedStats));
    REQUIRE(loadedStats.get<Float>(StatisticsId::RUN_TIME) == 24._f);
    REQUIRE(loadedStats.get<Float>(StatisticsId::TIMESTEP_VALUE) == 0.1_f);

    Expected<BinaryInput::Info> info = input.getInfo(path);
    REQUIRE(info);
    REQUIRE(info->runTime == 24._f);
    REQUIRE(info->timeStep == 0.1_f);
    REQUIRE(info->version == BinaryIoVersion::LATEST);
    REQUIRE(info->runType == RunTypeEnum::RUBBLE_PILE);
}

Storage generateLatestOutput() {
    BodySettings body1;
    body1.set(BodySettingsId::DENSITY, 1000._f);
    body1.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE);
    body1.set(BodySettingsId::BODY_CENTER, Vector(1._f, 2._f, 3._f));
    body1.set(BodySettingsId::DISTRIBUTE_MODE_SPH5, true);
    Storage storage1 = Tests::getSolidStorage(200, body1, 2._f);

    BodySettings body2;
    body2.set(BodySettingsId::DENSITY, 2000._f);
    body2.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::ELASTIC);
    body2.set(BodySettingsId::BODY_CENTER, Vector(0._f, 1._f, 2._f));
    body2.set(BodySettingsId::DISTRIBUTE_MODE_SPH5, false);
    Storage storage2 = Tests::getSolidStorage(30, body2, 1._f);

    Storage storage(std::move(storage1));
    storage.merge(std::move(storage2));

    Path path = RESOURCE_PATH / Path(std::to_string(std::size_t(BinaryIoVersion::LATEST)) + ".ssf");
    BinaryOutput output(path);
    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, 20._f);
    stats.set(StatisticsId::TIMESTEP_VALUE, 1.5_f);
    output.dump(storage, stats);
    return storage;
}

static void testVersion(BinaryIoVersion version) {
    Storage current = generateLatestOutput();
    Path path = RESOURCE_PATH / Path(std::to_string(std::size_t(version)) + ".ssf");
    BinaryInput input;
    Storage previous;
    Statistics stats;
    REQUIRE(input.load(path, previous, stats));

    REQUIRE(previous.getMaterialCnt() == current.getMaterialCnt());
    REQUIRE(previous.getParticleCnt() == current.getParticleCnt());
    REQUIRE(previous.getQuantityCnt() == current.getQuantityCnt());
    iteratePair<VisitorEnum::ALL_BUFFERS>(current, previous, [](auto& b1, auto& b2) { //
        // even though we do a lossless save, we allow some eps-difference since floats generated on different
        // machine can be slightly different due to different compiler optimizations, etc.
        REQUIRE(almostEqual(b1, b2));
    });

    for (Size matId = 0; matId < current.getMaterialCnt(); ++matId) {
        MaterialView mat1 = current.getMaterial(matId);
        MaterialView mat2 = previous.getMaterial(matId);
        REQUIRE(
            mat1->getParam<Float>(BodySettingsId::DENSITY) == mat2->getParam<Float>(BodySettingsId::DENSITY));
        REQUIRE(mat1->getParam<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING) ==
                mat2->getParam<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING));
        REQUIRE(mat1->getParam<Vector>(BodySettingsId::BODY_CENTER) ==
                mat2->getParam<Vector>(BodySettingsId::BODY_CENTER));
        REQUIRE(mat1->getParam<bool>(BodySettingsId::DISTRIBUTE_MODE_SPH5) ==
                mat2->getParam<bool>(BodySettingsId::DISTRIBUTE_MODE_SPH5));
    }
}

TEST_CASE("BinaryOutput backward compatibility", "[output]") {
    // generateLatestOutput();
    testVersion(BinaryIoVersion::FIRST);
    testVersion(BinaryIoVersion::V2018_04_07);
    testVersion(BinaryIoVersion::V2018_10_24);
}

TEST_CASE("Pkdgrav output", "[output]") {
    BodySettings settings;
    settings.set(BodySettingsId::ENERGY, 50._f);
    Storage storage = Tests::getGassStorage(100, settings);
    storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, 0);

    RandomPathManager manager;
    Path path = manager.getPath("out");
    PkdgravOutput output(path, PkdgravParams{});
    Statistics stats;
    output.dump(storage, stats);
    REQUIRE(FileSystem::fileSize(path) > 0);

    PkdgravParams params;
    params.vaporThreshold = 0.f;
    Path path2 = manager.getPath("out");
    PkdgravOutput output2(path2, std::move(params));
    output2.dump(storage, stats);
    REQUIRE(FileSystem::fileSize(path2) == 0);
}


TEST_CASE("Pkdgrav load", "[output]") {
    // hardcoded path to pkdgrav output
    Path path("/home/pavel/projects/astro/sph/external/sph_0.541_5_45/pkdgrav_run/ss.last.bt");
    if (!FileSystem::pathExists(path)) {
        SKIP_TEST;
    }

    Storage storage;
    Statistics stats;
    PkdgravInput io;
    REQUIRE(io.load(path, storage, stats));
    REQUIRE(storage.getParticleCnt() > 5000);

    // check that particles are sorted by masses
    ArrayView<Float> m = storage.getValue<Float>(QuantityId::MASS);
    Float lastM = LARGE;
    Float sumM = 0._f;
    bool sorted = true;
    for (Size i = 0; i < m.size(); ++i) {
        sumM += m[i];
        if (m[i] > lastM) {
            sorted = false;
        }
        lastM = m[i];
    }
    REQUIRE(sorted);

    // this particular simulation is the impact into 10km target with rho=2700 km/m^3, so the sum of the
    // fragments should be +- as massive as the target
    REQUIRE(sumM == approx(2700 * sphereVolume(5000), 1.e-3_f));

    /*Array<Float>& rho = storage->getValue<Float>(QuantityId::DENSITY);
    REQUIRE(perElement(rho) < 2800._f);
    REQUIRE(perElement(rho) > 2600._f);*/
}
