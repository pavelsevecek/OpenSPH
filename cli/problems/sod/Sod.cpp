/// \file Sod.cpp
/// \brief Sod shock tube test
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "Sph.h"
#include "catch.hpp"
#include "sod/Solution.h"
#include "sph/solvers/SummationSolver.h"

using namespace Sph;

INLINE Float smoothingFunc(const Float x, const Float x1, const Float x2) {
#if 0
    const Float w = exp(-(x - 0.5_f) / 0.0005_f);
    if (x > 0.52_f) {
        return x2;
    } else if (x < 0.48_f) {
        return x1;
    }
    return (x1 * w + x2) / (1._f * w + 1._f);
#else
    return (x > 0._f) ? x2 : x1;
#endif
}

Array<Vector> sodDistribution(const Float x1,
    const Float x2,
    const Float rho1,
    const Float rho2,
    const Float eta) {
    Array<Vector> r;
    Float delta = 0.005_f;
    Float dx1 = delta * rho2 / (rho1 + rho2);
    Float dx2 = delta * rho1 / (rho1 + rho2);
    Float dx = 0._f;
    for (Float x = x1; x <= x2; x += dx) {
        dx = (x < 0._f) ? dx1 : dx2;
        r.push(Vector(x, 0._f, 0._f, eta * dx));
    }
    return r;
}

class SodOutput : public IOutput {
private:
    AutoPtr<TextOutput> main;
    AutoPtr<TextOutput> analytic;

public:
    SodOutput(const std::string& name)
        : IOutput(Path("dummy")) {
        auto flags = OutputQuantityFlag::POSITION | OutputQuantityFlag::VELOCITY |
                     OutputQuantityFlag::DENSITY | OutputQuantityFlag::PRESSURE | OutputQuantityFlag::ENERGY;
        main = makeAuto<TextOutput>(Path("sod/sod_%d.txt"), name, flags);
        analytic = makeAuto<TextOutput>(Path("sod/sod_analytic_%d.txt"), name, flags);
    }

    virtual Expected<Path> dump(const Storage& storage, const Statistics& stats) override {
        const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
        SodConfig conf;
        Storage an = analyticSod(conf, t);
        analytic->dump(an, stats);
        return main->dump(storage, stats);
    }
};

class SodBc : public IBoundaryCondition {
private:
    SodConfig sod;
    Float eta = 0.01_f;

public:
    SodBc(const SodConfig& sod)
        : sod(sod) {}

    virtual void initialize(Storage& storage) override {
        this->reset(storage);
    }


    virtual void finalize(Storage& storage) override {
        this->reset(storage);
    }

private:
    void reset(Storage& storage) {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        ArrayView<Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
        ArrayView<Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
        ArrayView<Float> u = storage.getValue<Float>(QuantityId::ENERGY);
        ArrayView<Float> du = storage.getDt<Float>(QuantityId::ENERGY);
        EosMaterial& mat = dynamic_cast<EosMaterial&>(storage.getMaterial(0).material());
        for (Size i = 0; i < v.size(); ++i) {
            if (r[i][X] > 0.5_f - eta) {
                v[i] = dv[i] = Vector(0._f);
                rho[i] = sod.rho_r;
                p[i] = sod.P_r;
                u[i] = mat.getEos().getInternalEnergy(rho[i], p[i]);
                du[i] = 0._f;
            } else if (r[i][X] < -0.5_f + eta) {
                v[i] = dv[i] = Vector(0._f);
                rho[i] = sod.rho_l;
                p[i] = sod.P_l;
                u[i] = mat.getEos().getInternalEnergy(rho[i], p[i]);
                du[i] = 0._f;
            }
        }
    }
};

class Sod : public IRun {
public:
    Sod() {
        // Global settings of the problem
        this->settings.set(RunSettingsId::RUN_NAME, std::string("Sod Shock Tube Problem"))
            .set(RunSettingsId::RUN_END_TIME, 0.3_f)
            .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 0.02_f)
            .set(RunSettingsId::RUN_OUTPUT_PATH, std::string(""))
            .set(RunSettingsId::RUN_OUTPUT_NAME, std::string("sod_%d.txt"))
            .set(RunSettingsId::SPH_AV_ALPHA, 1._f)
            .set(RunSettingsId::SPH_AV_BETA, 2._f)
            .set(RunSettingsId::SPH_KERNEL, KernelEnum::CUBIC_SPLINE)
            .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
            .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-5_f)
            .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1.e-1_f)
            .set(RunSettingsId::TIMESTEPPING_COURANT_NUMBER, 0.2_f)
            .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::COURANT)
            .set(RunSettingsId::SPH_SOLVER_FORCES, ForceEnum::PRESSURE)
            .set(RunSettingsId::SPH_FINDER, FinderEnum::UNIFORM_GRID);
    }

    virtual void setUp(SharedPtr<Storage> storage) override {
        SodConfig conf;

        // Material properties
        BodySettings body;
        body.set(BodySettingsId::EOS, EosEnum::IDEAL_GAS)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE)
            .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE)
            .set(BodySettingsId::ADIABATIC_INDEX, conf.gamma)
            .set(BodySettingsId::SMOOTHING_LENGTH_ETA, 1.5_f)
            .set(BodySettingsId::DENSITY_RANGE, Interval(0.05_f, INFTY))
            .set(BodySettingsId::ENERGY_RANGE, Interval(0.05_f, INFTY))
            .set(BodySettingsId::DENSITY, 1._f)
            .set(BodySettingsId::DENSITY_MIN, 0.001_f)
            .set(BodySettingsId::ENERGY, 2.5_f)
            .set(BodySettingsId::ENERGY_MIN, 0.001_f);
        *storage = Storage(Factory::getMaterial(body));

        this->output = makeAuto<SodOutput>(this->settings.get<std::string>(RunSettingsId::RUN_NAME));

        // 1) setup initial positions, with different spacing in each region
        const Float x1 = -0.5_f;
        const Float x2 = 0.5_f;
        const Float eta = body.get<Float>(BodySettingsId::SMOOTHING_LENGTH_ETA);
        storage->insert<Vector>(
            QuantityId::POSITION, OrderEnum::SECOND, sodDistribution(x1, x2, conf.rho_l, conf.rho_r, eta));

        // 2) setup initial masses of particles
        storage->insert<Float>(QuantityId::MASS, OrderEnum::ZERO, 1._f);
        ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITION);
        ArrayView<Float> m = storage->getValue<Float>(QuantityId::MASS);
        for (Size i = 0; i < r.size(); ++i) {
            // mass = 1/N *integral density * dx
            m[i] = (-x1 * conf.rho_l + x2 * conf.rho_r) / r.size();
        }

        // 3) manually create the solver (no other way to get 1D solver right now),
        //    this also creates quantities (density, etc).
        this->solver =
            makeAuto<SummationSolver<1>>(*scheduler, settings, EquationHolder{}, makeAuto<SodBc>(conf));
        this->solver->create(*storage, storage->getMaterial(0));
        MaterialInitialContext context;
        context.scheduler = scheduler;
        EosMaterial& mat = dynamic_cast<EosMaterial&>(storage->getMaterial(0).material());
        mat.create(*storage, context);

        // 4) compute other quantities
        ArrayView<Float> p = storage->getValue<Float>(QuantityId::PRESSURE);
        ArrayView<Float> rho = storage->getValue<Float>(QuantityId::DENSITY);
        ArrayView<Float> u = storage->getValue<Float>(QuantityId::ENERGY);
        for (Size i = 0; i < r.size(); ++i) {
            p[i] = smoothingFunc(r[i][0], conf.P_l, conf.P_r);
            rho[i] = smoothingFunc(r[i][0], conf.rho_l, conf.rho_r);
            u[i] = mat.getEos().getInternalEnergy(rho[i], p[i]);
        }
        Statistics stats;
        solver->integrate(*storage, stats);


        // AutoPtr<IEos> eos = Factory::getEos(body);
        // u[i] = eos->getInternalEnergy(
        //  smoothingFunc(r[i][0], conf.rho_l, conf.rho_r), smoothingFunc(r[i][0], conf.P_l, conf.P_r));
    }

protected:
    virtual void tearDown(const Storage& UNUSED(storage), const Statistics& UNUSED(stats)) override {}
};

TEST_CASE("Sod", "[sod]") {
    // generate analytical solution
    /*  SodConfig conf;
      TextOutput output(Path("sod_analytical_%d.txt"),
          "sod",
          OutputQuantityFlag::POSITION | OutputQuantityFlag::VELOCITY | OutputQuantityFlag::DENSITY |
              OutputQuantityFlag::PRESSURE);
      Statistics stats;
      for (Float t = 0.1_f; t <= 0.5_f; t += 0.1_f) {
          Storage an = analyticSod(conf, t);
          stats.set(StatisticsId::RUN_TIME, t);
          output.dump(an, stats);
      }*/

    Sod run;
    Storage storage;
    run.run(storage);
}
