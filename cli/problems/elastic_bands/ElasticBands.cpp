/// \file ElasticBands.cpp
/// \brief Colliding elastic bands test
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "Common.h"
#include "catch.hpp"

using namespace Sph;

class ElasticBands : public IRun {
public:
    ElasticBands() {
        settings.set(RunSettingsId::RUN_NAME, std::string("Colliding Elastic Bands Problem"))
            .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
            .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-8_f)
            .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 100._f)
            .set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::BINARY_FILE)
            .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 4.e-4_f)
            .set(RunSettingsId::RUN_OUTPUT_PATH, std::string("elastic_bands"))
            .set(RunSettingsId::RUN_OUTPUT_NAME, std::string("bands_%d.ssf"))
            .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 4.e-3_f))
            .set(RunSettingsId::SOLVER_FORCES, ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS)
            .set(RunSettingsId::SOLVER_TYPE, SolverEnum::ASYMMETRIC_SOLVER)
            .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
            .set(RunSettingsId::SPH_DISCRETIZATION, DiscretizationEnum::STANDARD)
            .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
            .set(RunSettingsId::SPH_AV_USE_STRESS, true)
            .set(RunSettingsId::SPH_AV_STRESS_FACTOR, 0.04_f)
            .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
            .set(RunSettingsId::SPH_AV_BETA, 3._f)
            .set(RunSettingsId::SPH_KERNEL, KernelEnum::CUBIC_SPLINE)
            .set(RunSettingsId::SPH_KERNEL_ETA, 1.3_f)
            .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 0.2_f)
            .set(RunSettingsId::TIMESTEPPING_COURANT_NUMBER, 1._f)
            .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100)
            .set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST)
            .set(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, false)
            .set(RunSettingsId::FRAME_ANGULAR_FREQUENCY, Vector(0._f));
    }

    class SubtractDomain : public IDomain {
    private:
        IDomain& primary;
        IDomain& subtracted;

    public:
        SubtractDomain(IDomain& primary, IDomain& subtracted)
            : primary(primary)
            , subtracted(subtracted) {}

        virtual Vector getCenter() const override {
            return primary.getCenter();
        }

        virtual Box getBoundingBox() const override {
            return primary.getBoundingBox();
        }

        virtual Float getVolume() const override {
            return primary.getVolume() - subtracted.getVolume();
        }

        virtual bool contains(const Vector& v) const override {
            return primary.contains(v) && !subtracted.contains(v);
        }

        virtual void getSubset(ArrayView<const Vector>, Array<Size>&, const SubsetType) const override {
            NOT_IMPLEMENTED
        }

        virtual void getDistanceToBoundary(ArrayView<const Vector>, Array<Float>&) const override {
            NOT_IMPLEMENTED
        }

        virtual void project(ArrayView<Vector>, Optional<ArrayView<Size>>) const override {
            NOT_IMPLEMENTED
        }

        virtual void addGhosts(ArrayView<const Vector>,
            Array<Ghost>&,
            const Float,
            const Float) const override {
            NOT_IMPLEMENTED
        }
    };

    virtual void setUp() override {
        Size N = 10000;

        BodySettings body;
        body.set(BodySettingsId::ENERGY, 10._f)
            .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
            .set(BodySettingsId::EOS, EosEnum::MURNAGHAN)
            .set(BodySettingsId::SHEAR_MODULUS, 1.e9_f)
            .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::ELASTIC)
            .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, false)
            .set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::HEXAGONAL)
            .set(BodySettingsId::ENERGY_MIN, 100._f)
            .set(BodySettingsId::STRESS_TENSOR_MIN, 1.e6_f)
            .set(BodySettingsId::DAMAGE_MIN, 10._f)
            .set(BodySettingsId::PARTICLE_COUNT, int(N));

        storage = makeShared<Storage>();
        solver = Factory::getSolver(*scheduler, settings);
        InitialConditions ic(*scheduler, *solver, settings);

        CylindricalDomain outerRing(Vector(0._f), 0.04_f, 0.01_f, true);
        CylindricalDomain innerRing(Vector(0._f), 0.03_f, 0.01_f, true);
        SubtractDomain domain(outerRing, innerRing);
        ic.addMonolithicBody(*storage, domain, body)
            .displace(Vector(-0.06_f, 0._f, 0._f))
            .addVelocity(Vector(80._f, 0._f, 0._f));
        ic.addMonolithicBody(*storage, domain, body)
            .displace(Vector(0.06_f, 0._f, 0._f))
            .addVelocity(Vector(-80._f, 0._f, 0._f));

        triggers.pushBack(makeAuto<ProgressLog>(2.e-4_f));
    }

protected:
    virtual void tearDown(const Statistics& UNUSED(stats)) override {}
};

TEST_CASE("Elastic Bands", "[elastic]") {
    Array<Path> filesToCheck = { Path("elastic_bands/bands_0004.ssf"), Path("elastic_bands/bands_0009.ssf") };

    for (Path file : filesToCheck) {
        FileSystem::removePath(file);
    }

    ElasticBands run;
    run.setUp();
    run.run();

    for (Path file : filesToCheck) {
        REQUIRE(areFilesEqual(file, REFERENCE_DIR / file.fileName()));
    }
}
