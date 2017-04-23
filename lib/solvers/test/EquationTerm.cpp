#include "solvers/EquationTerm.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "objects/containers/PerElementWrapper.h"
#include "objects/finders/Voxel.h"
#include "solvers/GenericSolver.h"
#include "sph/initial/Distribution.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"

#include "io/Logger.h"

using namespace Sph;

/// \todo Move to some common place (utils.h)

struct TestDerivative : public Abstract::Derivative {
    static bool initialized;
    static bool created;

    ArrayView<Size> flags;

    virtual void create(Accumulated& results) override {
        results.insert<Size>(QuantityId::FLAG);
        created = true;
    }

    virtual void initialize(const Storage& UNUSED(input), Accumulated& results) override {
        flags = results.getValue<Size>(QuantityId::FLAG);
        initialized = true;
    }

    virtual void compute(const Size idx,
        ArrayView<const Size> UNUSED(neighs),
        ArrayView<const Vector> UNUSED(grads)) override {
        flags[idx]++;
    }
};
bool TestDerivative::initialized = false;
bool TestDerivative::created = false;

struct TestEquation : public Abstract::EquationTerm {
    virtual void setDerivatives(DerivativeHolder& derivatives) override {
        derivatives.require<TestDerivative>();
    }

    virtual void finalize(Storage& UNUSED(storage)) override {}

    virtual void create(Storage& UNUSED(storage), Abstract::Material& UNUSED(material)) const override {}
};

TEST_CASE("Setting derivatives", "[equationterm]") {
    TestDerivative::initialized = false;
    TestEquation eq;
    DerivativeHolder derivatives;
    eq.setDerivatives(derivatives);
    Storage storage;
    // add some dummy quantity to set particle count
    storage.insert<Size>(QuantityId::DAMAGE, OrderEnum::FIRST, Array<Size>{ 1, 2, 3, 4, 5 });

    // initialize, creating buffers and settings up arrayviews for derivatives
    derivatives.initialize(storage);
    derivatives.getAccumulated().store(storage);
    REQUIRE(storage.getParticleCnt() == 5);
    REQUIRE(TestDerivative::initialized);
}

static Storage getStorage(const Size particleCnt) {
    Storage storage(std::make_unique<NullMaterial>(BodySettings::getDefaults()));
    HexagonalPacking distribution;
    storage.insert<Vector>(QuantityId::POSITIONS,
        OrderEnum::SECOND,
        distribution.generate(particleCnt, SphericalDomain(Vector(0._f), 1._f)));
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 1._f);
    // density = 1, therefore total mass = volume, therefore mass per particle = volume / N
    storage.insert<Float>(QuantityId::MASSES, OrderEnum::ZERO, sphereVolume(1._f) / storage.getParticleCnt());
    REQUIRE(storage.getParticleCnt() > 0.9f * particleCnt); // sanity check
    return storage;
}

TEST_CASE("TestEquation", "[equationterm]") {
    Storage storage = getStorage(10);
    const Size N = storage.getParticleCnt();
    Statistics stats;
    EquationHolder equations(std::make_unique<TestEquation>());
    GenericSolver solver(RunSettings::getDefaults(), std::move(equations));
    solver.create(storage, storage.getMaterial(0));
    solver.integrate(storage, stats);

    ArrayView<Size> cnts = storage.getValue<Size>(QuantityId::FLAG);
    REQUIRE(cnts.size() == 10);
    // test equations only counts execution count, derivative should be executed once for each particle
    REQUIRE(perElement(cnts) == 1);
}

TEST_CASE("NeighbourCountTerm", "[equationterm]") {
    Storage storage = getStorage(10000);
    const Size N = storage.getParticleCnt();
    Statistics stats;
    EquationHolder equations;
    equations += makeTerm<NeighbourCountTerm>();
    GenericSolver solver(RunSettings::getDefaults(), std::move(equations));
    solver.create(storage, storage.getMaterial(0));

    solver.integrate(storage, stats);

    ArrayView<Size> neighCnts = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);

    REQUIRE(neighCnts.size() == N);
    // count neighbours manually and compare
    VoxelFinder finder;
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    finder.build(r);
    const Float radius = Factory::getKernel<3>(RunSettings::getDefaults()).radius();
    Array<NeighbourRecord> neighs;
    auto test = [&](Size i) {
        const Size cnt = finder.findNeighbours(i, r[i][H] * radius, neighs, EMPTY_FLAGS);
        if (cnt != neighCnts[i] + 1) {
            // +1 for the particle itself
            return makeFailed("Incorrect neighbour count for particle ", i, "\n", cnt, " == ", neighCnts[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}

TEST_CASE("Div v of position vectors", "[equationterm]") {
    // test case checking that div r = 3

    Storage storage = getStorage(10000);
    EquationHolder equations;
    equations += makeTerm<ContinuityEquation>(); // some term including velocity divergence
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
    for (Size i = 0; i < v.size(); ++i) {
        v[i] = r[i];
    }
    GenericSolver solver(RunSettings::getDefaults(), std::move(equations));
    solver.create(storage, storage.getMaterial(0));
    Statistics stats;
    solver.integrate(storage, stats);

    ArrayView<Float> divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
    REQUIRE(divv.size() == r.size());

    auto test = [&](const Size i) {
        // particles on boundary have different velocity divergence, check only particles inside
        if (getLength(r[i]) > 0.7_f) {
            return SUCCESS;
        }
        if (divv[i] != approx(3._f, 0.03_f)) {
            // clang-format off
            return makeFailed("Incorrect velocity divergence: \n",
                              "divv: ", divv[i], " == -3", "\n particle: r = ", r[i]);
            // clang-format on
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}
