/// \file KelvinHelmholtz.cpp
/// \brief Kelvin-Helmholtz instability
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "Sph.h"
#include "catch.hpp"
#include <iostream>

using namespace Sph;

class KelvinHelmholtzDistribution : public IDistribution {
public:
    virtual Array<Vector> generate(IScheduler&, const Size n, const IDomain& domain) const override {
        const Box box = domain.getBoundingBox();
        Interval inner(0.5_f * box.lower()[Y], 0.5_f * box.upper()[Y]);
        const Float h = sqrt(1._f / n);
        const Float dx = h;
        const Float dy = sqrt(3._f) / 2._f * dx;
        Array<Vector> r;
        int row = 0;
        Float factor = 1._f;
        Float prevFactor = 1._f;
        for (Float y = box.lower()[Y]; y <= box.upper()[Y]; y += factor * dy, row++) {
            factor = (inner.contains(y)) ? sqrt(2._f) / 2._f : 1._f;
            if (factor != prevFactor && y > 0._f) {
                y += (1._f - sqrt(2._f) / 2._f) * dy;
            }
            prevFactor = factor;

            for (Float x = box.lower()[X]; x <= box.upper()[X]; x += factor * dx) {
                Float delta = (row % 2) ? 0.5_f * factor * dx : 0._f;
                r.push(Vector(x + delta, y, 0._f, factor * h));
            }
        }
        return r;
    }
};

class KelvinHelmholtz : public IRun {
public:
    KelvinHelmholtz() {
        // Global settings of the problem
        this->settings.set(RunSettingsId::RUN_NAME, std::string("Kelvin-Helmholtz instability"))
            .set(RunSettingsId::RUN_END_TIME, 8._f)
            .set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::TEXT_FILE)
            .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 0.1_f)
            .set(RunSettingsId::RUN_OUTPUT_PATH, std::string(""))
            .set(RunSettingsId::RUN_OUTPUT_NAME, std::string("kh/kh_%d.txt"))
            .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
            .set(RunSettingsId::SPH_AV_BETA, 3._f)
            .set(RunSettingsId::SPH_USE_AC, true)
            .set(RunSettingsId::SPH_KERNEL, KernelEnum::CUBIC_SPLINE)
            .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::EULER_EXPLICIT)
            .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-5_f)
            .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 0.00002_f)
            .set(RunSettingsId::TIMESTEPPING_COURANT_NUMBER, 0.2_f)
            .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::COURANT)
            .set(RunSettingsId::SPH_SOLVER_FORCES, ForceEnum::PRESSURE)
            .set(RunSettingsId::SPH_FINDER, FinderEnum::UNIFORM_GRID)
            .set(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH, EMPTY_FLAGS)
            .set(RunSettingsId::DOMAIN_TYPE, DomainEnum::BLOCK)
            .set(RunSettingsId::DOMAIN_BOUNDARY, BoundaryEnum::PERIODIC)
            //.set(RunSettingsId::DOMAIN_BOUNDARY, BoundaryEnum::FROZEN_PARTICLES)
            .set(RunSettingsId::DOMAIN_SIZE, Vector(1.01_f, 1._f, 1._f));
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
            .set(BodySettingsId::ADIABATIC_INDEX, 1.4_f)
            .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, true);

        *storage = Storage(Factory::getMaterial(body));
        AutoPtr<IDistribution> dist = makeAuto<KelvinHelmholtzDistribution>();
        BlockDomain domain(Vector(0._f), Vector(1._f, 1._f, 1.e-3_f));
        Array<Vector> pos = dist->generate(*scheduler, 5000, domain);
        const Float eta = 1.5_f;
        Size highCnt = 0._f;
        for (Size i = 0; i < pos.size(); ++i) {
            pos[i][H] *= eta;
            highCnt += int(abs(pos[i][Y]) < 0.25_f);
        }
        // rho * S / N ??
        const Float m = 2._f * body.get<Float>(BodySettingsId::DENSITY) * 0.5_f / highCnt;

        storage->insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(pos));
        storage->insert<Float>(QuantityId::MASS, OrderEnum::ZERO, m);

        EquationHolder eqs = getStandardEquations(settings);
        this->solver = makeAuto<SymmetricSolver<2>>(*scheduler, settings, eqs);
        IMaterial& mat = storage->getMaterial(0);
        solver->create(*storage, mat);
        MaterialInitialContext context(settings);
        mat.create(*storage, context);

        ArrayView<const Vector> r = storage->getValue<Vector>(QuantityId::POSITION);
        ArrayView<Vector> v = storage->getDt<Vector>(QuantityId::POSITION);
        ArrayView<Float> rho = storage->getValue<Float>(QuantityId::DENSITY);
        ArrayView<Float> u = storage->getValue<Float>(QuantityId::ENERGY);
        EosMaterial& eosmat = dynamic_cast<EosMaterial&>(mat);
        for (Size i = 0; i < rho.size(); ++i) {
            if (abs(r[i][Y]) < 0.25_f) {
                rho[i] *= 2._f;
                v[i][X] = 0.5_f;
            } else {
                v[i][X] = -0.5_f;
            }
            const Float A = 0.025_f;
            const Float lambda = 1._f / 6._f;
            if (abs(r[i][Y] - 0.25_f) < 0.025_f) {
                v[i][Y] = A * sin(-2._f * PI * (r[i][X] + 1._f) / lambda);
            } else if (abs(r[i][Y] + 0.25_f) < 0.025_f) {
                v[i][Y] = A * sin(2._f * PI * (r[i][X] + 1._f) / lambda);
            }
            u[i] = eosmat.getEos().getInternalEnergy(rho[i], 2.5_f);
        }
    }

protected:
    virtual void tearDown(const Storage& UNUSED(storage), const Statistics& UNUSED(stats)) override {}
};

TEST_CASE("Kelvin-Helmholtz", "[kh]") {
    KelvinHelmholtz run;
    Storage storage;
    run.run(storage);
}
