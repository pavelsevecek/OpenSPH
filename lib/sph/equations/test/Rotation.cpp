#include "sph/equations/Rotation.h"
#include "catch.hpp"
#include "io/Column.h"
#include "io/Output.h"
#include "math/Functional.h"
#include "physics/Eos.h"
#include "run/IRun.h"
#include "run/RunCallbacks.h"
#include "sph/boundary/Boundary.h"
#include "sph/solvers/SymmetricSolver.h"
#include "tests/Approx.h"
#include "tests/Setup.h"
#include "timestepping/TimeStepping.h"

using namespace Sph;

namespace {
class TestRun : public IRun {
private:
    EquationHolder equations;
    Size observedIndex;

public:
    class Callbacks : public IRunCallbacks {
    private:
        Size observedIndex;
        FileLogger logger;
        Size stepIdx = 0;

    public:
        explicit Callbacks(const Size observedIndex)
            : observedIndex(observedIndex)
            , logger(Path("rotation.txt"), EMPTY_FLAGS) {}

        virtual void onTimeStep(const Storage& storage, Statistics& UNUSED(stats)) override {
            ArrayView<const Vector> phi = storage.getValue<Vector>(QuantityId::PHASE_ANGLE);
            ArrayView<const Vector> omega = storage.getValue<Vector>(QuantityId::ANGULAR_VELOCITY);
            ArrayView<const Float> u = storage.getValue<Float>(QuantityId::ENERGY);
            logger.write(stepIdx++, phi[observedIndex], omega[observedIndex], "  ", u[observedIndex]);
        }

        virtual void onRunStart(const Storage& UNUSED(storage), Statistics& UNUSED(stats)) override {}

        virtual void onRunEnd(const Storage& UNUSED(storage), Statistics& UNUSED(stats)) override {}

        virtual bool shouldAbortRun() const override {
            return false;
        }
    };
    /*
            class ZeroRotation : public IEquationTerm {
            private:
                Size observedIndex;

            public:
                explicit ZeroRotation(const Size observedIndex)
                    : observedIndex(observedIndex) {}

                virtual void setDerivatives(DerivativeHolder&, const RunSettings&) override {}

                virtual void initialize(Storage&) override {}

                virtual void finalize(Storage& storage) override {
                    ArrayView<Vector> omega = storage.getValue<Vector>(QuantityId::ANGULAR_VELOCITY);
                    ArrayView<Vector> dphi = storage.getDt<Vector>(QuantityId::PHASE_ANGLE);
                    for (Size i = 0; i < omega.size(); ++i) {
                        if (i != observedIndex) {
                            omega[i] = dphi[i] = Vector(0._f);
                        }
                    }
                }

                virtual void create(Storage&, IMaterial&) const override {}
            };*/

    TestRun(const SharedPtr<Storage>& storage, const Interval timeline, const Size observedIndex) {

        this->storage = storage;
        this->observedIndex = observedIndex;

        settings.set(RunSettingsId::RUN_TIME_RANGE, timeline);

        const Float duration = timeline.size();
        settings.set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 0.0001_f * duration)
            .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.0001_f * duration)
            .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 0.01_f * duration)
            .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::COURANT)
            .set(RunSettingsId::SPH_PHASE_ANGLE, true)
            .set(RunSettingsId::SPH_PARTICLE_ROTATION, true);

        /*AutoPtr<TextOutput> textOutput = makeAuto<TextOutput>(Path("out_%d.txt"), "rot");

        textOutput->addColumn(makeAuto<ValueColumn<Vector>>(QuantityId::POSITION));
        textOutput->addColumn(makeAuto<ValueColumn<Vector>>(QuantityId::ANGULAR_VELOCITY));

        output = std::move(textOutput);*/

        equations += makeTerm<SolidStressForce>(
            settings); /// \todo FIX ROTATION+ makeTerm<SolidStressTorque>(settings);
        equations += makeTerm<ConstSmoothingLength>();
        // add boundary conditions last
        //   equations += makeTerm<FrozenParticles>(makeAuto<SphericalDomain>(Vector(0._f), 1._f), 2._f);
    }

    virtual void setUp() override {
        solver = makeAuto<SymmetricSolver>(settings, equations);
        IMaterial& material = storage->getMaterial(0);
        solver->create(*storage, material);

        timeStepping = makeAuto<EulerExplicit>(storage, settings);
        callbacks = makeAuto<Callbacks>(observedIndex);
    }

    virtual void tearDown(const Statistics& UNUSED(stats)) override {}
};
} // namespace


/*TEST_CASE("Rotation vibrations", "[rotation]") {
    BodySettings body;
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE);
    body.set(BodySettingsId::RHEOLOGY_DAMAGE, DamageEnum::NONE);
    SharedPtr<Storage> storage = makeShared<Storage>(Tests::getSolidStorage(50, body));

    //const Float rho0 = body.get<Float>(BodySettingsId::DENSITY);
    /const Float mu0 = body.get<Float>(BodySettingsId::SHEAR_MODULUS);
    //const Float h0 = storage->getValue<Vector>(QuantityId::POSITIONS)[0][H];
    //const Float u0 = body.get<Float>(BodySettingsId::ENERGY);

    //TillotsonEos eos(body);
    //Float cs, p;
    //tie(p, cs) = eos.evaluate(rho0, u0);

    // add rotation quantities and spin up the center particle
    storage->insert<Vector>(QuantityId::PHASE_ANGLE, OrderEnum::FIRST, Vector(0._f));
    storage->insert<Vector>(QuantityId::ANGULAR_VELOCITY, OrderEnum::FIRST, Vector(0._f));
    const Size centerIdx = Tests::getClosestParticle(*storage, Vector(0._f));
    ArrayView<Vector> omega = storage->getValue<Vector>(QuantityId::ANGULAR_VELOCITY);
    // omega[centerIdx] = Vector(0._f, 0._f, 100._f);
    omega[centerIdx] = Vector(0._f, 0._f, 1._f);

    const Float t = 0.05_f;

    // run the simulation
    TestRun run(storage, Interval(0._f, t), centerIdx);
    run.setUp();
    run.run();
}*/

TEST_CASE("Rotation inertia", "[rotation]") {
    Integrator<UniformRng> integrator(makeAuto<SphericalDomain>(Vector(0._f), 2._f));
    LutKernel<3> kernel(CubicSpline<3>{});
    const Float value = integrator.integrate([&kernel](const Vector& r) { //
        return (sqr(r[X]) + sqr(r[Y])) * kernel.value(r, 1._f);
    });
    REQUIRE(value == approx(0.6_f, 0.01_f));
}
