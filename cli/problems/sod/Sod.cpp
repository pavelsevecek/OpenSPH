/// \file Sod.cpp
/// \brief Sod shock tube test
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "Sph.h"
#include "catch.hpp"

using namespace Sph;

INLINE Float smoothingFunc(const Float x, const Float x1, const Float x2) {
    const Float w = exp(-(x - 0.5_f) / 0.0005_f);
    if (x > 0.52_f) {
        return x2;
    } else if (x < 0.48_f) {
        return x1;
    }
    return (x1 * w + x2) / (1._f * w + 1._f);
}

Array<Vector> sodDistribution(const int N, Float dx, const Float eta) {
    Array<Vector> x(N);
    do {
        float actX = 0.f;
        for (Size i = 0; i < x.size(); ++i) {
            x[i][0] = actX;
            const float actDx = smoothingFunc(x[i][0], dx, dx / 0.125_f);
            actX += actDx;
            x[i][H] = eta * actDx;
        }
        ASSERT(x.size() > 0);
        if (x[x.size() - 1][0] > 1._f) {
            dx -= 0.001_f / N;
        } else {
            dx += 0.001_f / N;
        }
    } while (!Interval(0.99f, 1.01f).contains(x[x.size() - 1][0]));
    return x;
}

class Sod : public IRun {
public:
    Sod() {
        // Global settings of the problem
        this->settings.set(RunSettingsId::RUN_NAME, std::string("Sod Shock Tube Problem"))
            .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 0.5_f))
            .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 0.02_f)
            .set(RunSettingsId::RUN_OUTPUT_PATH, std::string(""))
            .set(RunSettingsId::RUN_OUTPUT_NAME, std::string("sod_%d.ssf"))
            .set(RunSettingsId::DOMAIN_TYPE, DomainEnum::SPHERICAL)
            .set(RunSettingsId::DOMAIN_CENTER, Vector(0.5_f))
            .set(RunSettingsId::DOMAIN_RADIUS, 0.5_f)
            .set(RunSettingsId::DOMAIN_BOUNDARY, BoundaryEnum::PROJECT_1D)
            .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::EULER_EXPLICIT)
            .set(RunSettingsId::SPH_AV_ALPHA, 1.0_f)
            .set(RunSettingsId::SPH_AV_BETA, 2.0_f)
            .set(RunSettingsId::SPH_KERNEL_ETA, 1.5_f)
            .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
            .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-5_f)
            .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1.e-1_f)
            .set(RunSettingsId::TIMESTEPPING_COURANT_NUMBER, 0.5_f)
            .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::COURANT)
            .set(RunSettingsId::SOLVER_TYPE, SolverEnum::SYMMETRIC_SOLVER)
            .set(RunSettingsId::SOLVER_FORCES, ForceEnum::PRESSURE);
    }

    virtual void setUp() override {
        storage = makeShared<Storage>();

        // Number of SPH particles
        const int N = 400;
        // Material properties
        BodySettings body;
        body.set(BodySettingsId::PARTICLE_COUNT, N)
            .set(BodySettingsId::EOS, EosEnum::IDEAL_GAS)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE)
            .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE)
            .set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::LINEAR)
            .set(BodySettingsId::ADIABATIC_INDEX, 1.4_f)
            .set(BodySettingsId::DENSITY_RANGE, Interval(0.05_f, INFTY))
            .set(BodySettingsId::ENERGY_RANGE, Interval(0.05_f, INFTY))
            .set(BodySettingsId::DENSITY, 1._f)
            .set(BodySettingsId::DENSITY_MIN, 0.1_f)
            .set(BodySettingsId::ENERGY, 2.5_f)
            .set(BodySettingsId::ENERGY_MIN, 0.1_f);

        InitialConditions initialConditions(*scheduler, this->settings);
        initialConditions.addMonolithicBody(*storage, SphericalDomain(Vector(0.5_f), 0.5_f), body);

        Path outputDir("sod/" + this->settings.get<std::string>(RunSettingsId::RUN_OUTPUT_NAME));
        this->output = makeAuto<GnuplotOutput>(outputDir,
            this->settings.get<std::string>(RunSettingsId::RUN_NAME),
            "sod.plt",
            OutputQuantityFlag::POSITION,
            GnuplotOutput::Options::SCIENTIFIC);

        // 1) setup initial positions, with different spacing in each region
        const Float eta = this->settings.get<Float>(RunSettingsId::SPH_KERNEL_ETA);
        this->storage->getValue<Vector>(QuantityId::POSITION) = sodDistribution(N, 1._f / N, eta);

        // 2) setup initial pressure and masses of particles
        ArrayView<Vector> r;
        ArrayView<Float> p, m;
        r = storage->getValue<Vector>(QuantityId::POSITION);
        tie(p, m) = storage->getValues<Float>(QuantityId::PRESSURE, QuantityId::MASS);
        for (Size i = 0; i < N; ++i) {
            p[i] = smoothingFunc(r[i][0], 1._f, 0.1_f);
            // mass = 1/N *integral density * dx
            m[i] = 0.5_f * (1._f + 0.125_f) / N;
        }

        // 3) setup density to be consistent with masses
        AutoPtr<IBasicFinder> finder = Factory::getFinder(this->settings);
        finder->build(*scheduler, storage->getValue<Vector>(QuantityId::POSITION));
        LutKernel<1> kernel = Factory::getKernel<1>(settings);
        Array<NeighbourRecord> neighs;
        ArrayView<Float> rho = storage->getValue<Float>(QuantityId::DENSITY);
        for (Size i = 0; i < N; ++i) {
            if (r[i][X] < 0.15_f) {
                rho[i] = 1._f;
            } else if (r[i][X] > 0.85_f) {
                rho[i] = 0.125_f;
            } else {
                finder->findAll(i, r[i][H] * kernel.radius(), neighs);
                rho[i] = 0._f;
                for (Size n = 0; n < neighs.size(); ++n) {
                    const Size j = neighs[n].index;
                    rho[i] += m[j] * kernel.value(r[i] - r[j], r[i][H]);
                }
            }
        }

        // 4) compute internal energy using equation of state
        AutoPtr<IEos> eos = Factory::getEos(body);
        ArrayView<Float> u = storage->getValue<Float>(QuantityId::ENERGY);
        for (Size i = 0; i < N; ++i) {
            u[i] = eos->getInternalEnergy(
                smoothingFunc(r[i][0], 1._f, 0.125_f), smoothingFunc(r[i][0], 1._f, 0.1_f));
        }

        // 5) compute energy per particle and energy density if we are using DISPH
        /// \todo this should be somehow computed automatically
        if (settings.get<SolverEnum>(RunSettingsId::SOLVER_TYPE) == SolverEnum::DENSITY_INDEPENDENT) {
            ArrayView<Float> q, e;
            tie(q, e) =
                storage->getValues<Float>(QuantityId::ENERGY_DENSITY, QuantityId::ENERGY_PER_PARTICLE);
            for (Size i = 0; i < N; ++i) {
                // update 'internal' quantities in case 'external' quantities (density, specific energy, ...)
                // have been changed outside of the solver.
                q[i] = rho[i] * u[i];
                e[i] = m[i] * u[i];
            }
        }
    }

protected:
    virtual void tearDown(const Statistics& UNUSED(stats)) override {}
};

TEST_CASE("Sod", "[sod]") {
    Sod run;
    run.setUp();
    if (false) {
        // currently doesn't work, there is no easy way to run 1D simulation
        run.run();
    }
}
