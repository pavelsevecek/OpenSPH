/// \file Boundary.cpp
/// \brief Testing several boundary modes
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "Sph.h"
#include "catch.hpp"
#include "sph/solvers/StabilizationSolver.h"
#include "sph/solvers/SummationSolver.h"

using namespace Sph;

class Distribution : public IDistribution {
public:
    virtual Array<Vector> generate(IScheduler&, const Size n, const IDomain& domain) const override {
        const Box box = domain.getBoundingBox();
        const Float h = sqrt(1._f / n);
        const Float dx = h;
        const Float dy = h; // sqrt(3._f) / 2._f * dx;
        Array<Vector> r;
        int row = 0;
        for (Float y = box.lower()[Y] + 0.5f * dy; y <= box.upper()[Y]; y += dy, row++) {
            for (Float x = box.lower()[X] + 0.5f * dx; x <= box.upper()[X]; x += dx) {
                // Float delta = (row % 2) ? 0.5_f * dx : 0._f;
                r.push(Vector(x /*+ delta*/, y, 0._f, h));
            }
        }
        return r;
    }
};

class BoundaryRun : public IRun {
private:
    AutoPtr<IBoundaryCondition> bc;

public:
    BoundaryRun(AutoPtr<IBoundaryCondition>&& bc, const std::string& name)
        : BoundaryRun(BoundaryEnum::NONE, name) {
        this->bc = std::move(bc);
    }

    BoundaryRun(const BoundaryEnum boundary, const std::string& name) {
        // Global settings of the problem
        this->settings.set(RunSettingsId::RUN_NAME, name)
            .set(RunSettingsId::RUN_END_TIME, 4._f)
            .set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::COMPRESSED_FILE)
            .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 0.01_f)
            .set(RunSettingsId::RUN_OUTPUT_QUANTITIES,
                OutputQuantityFlag::POSITION | OutputQuantityFlag::DENSITY)
            .set(RunSettingsId::RUN_OUTPUT_PATH, std::string(""))
            .set(RunSettingsId::RUN_OUTPUT_NAME, "boundary/" + name + "_%d.scf")
            .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
            .set(RunSettingsId::SPH_AV_BETA, 3._f)
            .set(RunSettingsId::SPH_KERNEL, KernelEnum::CUBIC_SPLINE)
            .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
            .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-3_f)
            .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1.e-3_f)
            .set(RunSettingsId::TIMESTEPPING_COURANT_NUMBER, 0.05_f)
            .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::COURANT)
            .set(RunSettingsId::SPH_SOLVER_FORCES, ForceEnum::PRESSURE)
            .set(RunSettingsId::SPH_FINDER, FinderEnum::UNIFORM_GRID)
            .set(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH, EMPTY_FLAGS)
            .set(RunSettingsId::DOMAIN_TYPE, DomainEnum::BLOCK)
            .set(RunSettingsId::DOMAIN_BOUNDARY, boundary)
            //.set(RunSettingsId::DOMAIN_BOUNDARY, BoundaryEnum::FROZEN_PARTICLES)
            .set(RunSettingsId::DOMAIN_SIZE, Vector(1._f, 1._f, 1._f));
    }

    virtual void setUp(SharedPtr<Storage> storage) override {
        BodySettings body;
        body.set(BodySettingsId::DENSITY, 100._f)
            .set(BodySettingsId::DENSITY_RANGE, Interval(1.e-3_f, INFTY))
            .set(BodySettingsId::ENERGY, 0.25_f)
            .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
            .set(BodySettingsId::EOS, EosEnum::IDEAL_GAS)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE)
            .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE)
            .set(BodySettingsId::ADIABATIC_INDEX, 1.4_f)
            .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, true);

        *storage = Storage(Factory::getMaterial(body));
        AutoPtr<IDistribution> dist = makeAuto<Distribution>();
        BlockDomain domain(Vector(0._f), Vector(1._f, 1._f, 1.e-3_f));
        Array<Vector> pos = dist->generate(*scheduler, 40000, domain);
        const Float eta = 1.3_f;
        for (Size i = 0; i < pos.size(); ++i) {
            pos[i][H] *= eta;
        }
        // rho * S / N ??
        const Float m = body.get<Float>(BodySettingsId::DENSITY) / pos.size();

        storage->insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(pos));
        storage->insert<Float>(QuantityId::MASS, OrderEnum::ZERO, m);

        EquationHolder eqs = getStandardEquations(settings);
        if (bc) {
            this->solver = makeAuto<SymmetricSolver<2>>(*scheduler, settings, eqs, std::move(bc));
        } else {
            this->solver = makeAuto<SymmetricSolver<2>>(*scheduler, settings, eqs);
        }
        IMaterial& mat = storage->getMaterial(0);
        solver->create(*storage, mat);
        MaterialInitialContext context(settings);
        mat.create(*storage, context);

        ArrayView<const Vector> r = storage->getValue<Vector>(QuantityId::POSITION);
        ArrayView<Float> u = storage->getValue<Float>(QuantityId::ENERGY);
        Vector center(0.25, 0, 0);
        for (Size i = 0; i < r.size(); ++i) {
            if (getLength(r[i] - center) < 0.01_f) {
                u[i] = 5._f;
            }
        }
    }

protected:
    virtual void tearDown(const Storage& UNUSED(storage), const Statistics& UNUSED(stats)) override {}
};

TEST_CASE("Boundary test", "[boundary]") {
    Storage storage;
    RunSettings settings;
    settings.set(RunSettingsId::DOMAIN_TYPE, DomainEnum::BLOCK)
        .set(RunSettingsId::DOMAIN_SIZE, Vector(1._f, 1._f, 1._f));

    BoundaryRun periodic(BoundaryEnum::PERIODIC, "periodic");
    periodic.run(storage);

    BoundaryRun ghosts(BoundaryEnum::GHOST_PARTICLES, "ghosts");
    ghosts.run(storage);

    BoundaryRun frozen(BoundaryEnum::FROZEN_PARTICLES, "frozen");
    frozen.run(storage);
}
