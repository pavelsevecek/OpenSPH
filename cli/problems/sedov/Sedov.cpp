/// \file Sedov.cpp
/// \brief Sedov blast test
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "Sph.h"
#include "catch.hpp"
#include <iostream>

using namespace Sph;

class PlanarDistribution : public IDistribution {
public:
    virtual Array<Vector> generate(IScheduler&, const Size n, const IDomain& domain) const override {
        const Box box = domain.getBoundingBox();
        const Float h = sqrt(1._f / n);
        const Float dx = h;
        const Float dy = sqrt(3._f) / 2._f * dx;
        Array<Vector> r;
        int row = 0;
        for (Float y = box.lower()[Y]; y <= box.upper()[Y]; y += dy, row++) {
            for (Float x = box.lower()[X]; x <= box.upper()[X]; x += dx) {
                Float delta = (row % 2) ? 0.5_f * dx : 0._f;
                r.push(Vector(x + delta, y, 0._f, h));
            }
        }
        return r;
    }
};

class Sedov : public IRun {
public:
    Sedov() {
        // Global settings of the problem
        this->settings.set(RunSettingsId::RUN_NAME, String("Sedov Blast Problem"))
            .set(RunSettingsId::RUN_END_TIME, 8._f)
            .set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::TEXT_FILE)
            .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 0.08_f)
            .set(RunSettingsId::RUN_OUTPUT_PATH, String(""))
            .set(RunSettingsId::RUN_OUTPUT_NAME, String("sedov/sedov_%d.txt"))
            .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
            .set(RunSettingsId::SPH_AV_BETA, 3._f)
            .set(RunSettingsId::SPH_KERNEL, KernelEnum::CUBIC_SPLINE)
            .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::EULER_EXPLICIT)
            .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-5_f)
            .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 0.1_f)
            .set(RunSettingsId::TIMESTEPPING_COURANT_NUMBER, 0.2_f)
            .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::COURANT)
            .set(RunSettingsId::SPH_SOLVER_FORCES, ForceEnum::PRESSURE)
            .set(RunSettingsId::SPH_USE_AC, true)
            .set(RunSettingsId::SPH_FINDER, FinderEnum::UNIFORM_GRID)
            .set(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONTINUITY_EQUATION)
            .set(RunSettingsId::DOMAIN_TYPE, DomainEnum::BLOCK)
            //.set(RunSettingsId::DOMAIN_BOUNDARY, BoundaryEnum::FROZEN_PARTICLES)
            .set(RunSettingsId::DOMAIN_SIZE, Vector(1._f));
    }

    virtual void setUp(SharedPtr<Storage> storage) override {
        BodySettings body;
        body.set(BodySettingsId::DENSITY, 1._f)
            .set(BodySettingsId::DENSITY_RANGE, Interval(1.e-3_f, INFTY))
            .set(BodySettingsId::ENERGY, 0._f)
            .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
            .set(BodySettingsId::EOS, EosEnum::IDEAL_GAS)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE)
            .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE)
            .set(BodySettingsId::ADIABATIC_INDEX, 1.6666666666_f)
            .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, true);

        *storage = Storage(Factory::getMaterial(body));
        AutoPtr<IDistribution> dist = makeAuto<PlanarDistribution>(); // Factory::getDistribution(body);
        BlockDomain domain(Vector(0._f), Vector(1._f, 1._f, 1.e-3_f));
        Array<Vector> pos = dist->generate(*scheduler, 100000, domain);
        const Float eta = 1.5_f;
        for (Size i = 0; i < pos.size(); ++i) {
            pos[i][H] *= eta;
        }
        const Float m = body.get<Float>(BodySettingsId::DENSITY) / pos.size();

        storage->insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(pos));
        storage->insert<Float>(QuantityId::MASS, OrderEnum::ZERO,
            m); // rho * S / N

        EquationHolder eqs = getStandardEquations(settings);
        this->solver = makeAuto<SymmetricSolver<2>>(*scheduler, settings, eqs);
        IMaterial& mat = storage->getMaterial(0);
        solver->create(*storage, mat);
        MaterialInitialContext context(settings);
        mat.create(*storage, context);

        ArrayView<Float> u = storage->getValue<Float>(QuantityId::ENERGY);
        ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITION);
        Float E0 = 0._f;
        for (Size i = 0; i < r.size(); ++i) {
            if (getLength(r[i]) < 0.015_f) {
                u[i] = 4._f;
                E0 += u[i] * m;
            }
        }
        std::cout << "E0 = " << E0 << std::endl;
    }

protected:
    virtual void tearDown(const Storage& UNUSED(storage), const Statistics& UNUSED(stats)) override {}
};

TEST_CASE("Sedov", "[sedov]") {
    Sedov run;
    Storage storage;
    run.run(storage);
}
