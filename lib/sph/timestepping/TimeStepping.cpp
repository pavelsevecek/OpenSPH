#include "sph/timestepping/TimeStepping.h"
#include "quantities/Iterate.h"
#include "solvers/AbstractSolver.h"
#include "sph/timestepping/TimeStepCriterion.h"
#include "system/Factory.h"
#include "system/Profiler.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN


Abstract::TimeStepping::TimeStepping(const std::shared_ptr<Storage>& storage, const GlobalSettings& settings)
    : storage(storage) {
    dt = settings.get<Float>(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP);
    maxdt = settings.get<Float>(GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP);
    adaptiveStep = Factory::getTimeStepCriterion(settings);
}

Abstract::TimeStepping::~TimeStepping() = default;

void Abstract::TimeStepping::step(Abstract::Solver& solver, Statistics& stats) {
    this->stepImpl(solver);
    // update time step
    if (adaptiveStep) {
        QuantityIds criterion;
        tieToTuple(dt, criterion) = adaptiveStep->compute(*storage, maxdt);
        stats.set(StatisticsIds::TIMESTEP_VALUE, dt);
        stats.set(StatisticsIds::TIMESTEP_CRITERION, criterion);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// EulerExplicit implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void EulerExplicit::stepImpl(Abstract::Solver& solver) {
    MEASURE_SCOPE("EulerExplicit::step");
    // clear derivatives from previous timestep
    this->storage->init();

    // compute derivatives
    solver.integrate(*this->storage);

    PROFILE_SCOPE("EulerExplicit::step")
    // advance all 2nd-order quantities by current timestep, first values, then 1st derivatives
    iterate<VisitorEnum::SECOND_ORDER>(
        *this->storage, [this](const QuantityIds UNUSED(id), auto& v, auto& dv, auto& d2v) {
            for (Size i = 0; i < v.size(); ++i) {
                dv[i] += d2v[i] * this->dt;
                v[i] += dv[i] * this->dt;
            }
        });
    iterate<VisitorEnum::FIRST_ORDER>(
        *this->storage, [this](const QuantityIds UNUSED(id), auto& v, auto& dv) {
            for (Size i = 0; i < v.size(); ++i) {
                v[i] += dv[i] * this->dt;
            }
        });
    // clamp quantities
    /// \todo better
    for (auto& q : *this->storage) {
        q.second.clamp();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PredictorCorrector implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PredictorCorrector::PredictorCorrector(const std::shared_ptr<Storage>& storage,
    const GlobalSettings& settings)
    : Abstract::TimeStepping(storage, settings) {
    ASSERT(storage->getQuantityCnt() > 0); // quantities must already been emplaced
    predictions = std::make_unique<Storage>(storage->clone(VisitorEnum::HIGHEST_DERIVATIVES));
    storage->init(); // clear derivatives before using them in step method
}

PredictorCorrector::~PredictorCorrector() = default;

void PredictorCorrector::stepImpl(Abstract::Solver& solver) {
    const Float dt2 = 0.5_f * sqr(this->dt);

    PROFILE_SCOPE("PredictorCorrector::step   Predictions")
    // make prediction using old derivatives (simple euler)
    iterate<VisitorEnum::SECOND_ORDER>(
        *this->storage, [this, dt2](const QuantityIds UNUSED(id), auto& v, auto& dv, auto& d2v) {
            for (Size i = 0; i < v.size(); ++i) {
                v[i] += dv[i] * this->dt + d2v[i] * dt2;
                dv[i] += d2v[i] * this->dt;
            }
        });
    iterate<VisitorEnum::FIRST_ORDER>(
        *this->storage, [this](const QuantityIds UNUSED(id), auto& v, auto& dv) {
            for (Size i = 0; i < v.size(); ++i) {
                v[i] += dv[i] * this->dt;
            }
        });
    // clamp quantities
    /// \todo better
    for (auto& q : *this->storage) {
        q.second.clamp();
    }
    // save derivatives from predictions
    this->storage->swap(*predictions, VisitorEnum::HIGHEST_DERIVATIVES);

    // clear derivatives
    this->storage->init();
    SCOPE_STOP;
    // compute derivative
    solver.integrate(*this->storage);

    PROFILE_NEXT("PredictorCorrector::step   Corrections");
    // make corrections
    // clang-format off
    iteratePair<VisitorEnum::SECOND_ORDER>(*this->storage, *this->predictions,
        [this, dt2](auto& pv, auto& pdv, auto& pd2v, auto& UNUSED(cv), auto& UNUSED(cdv), auto& cd2v) {
        ASSERT(pv.size() == pd2v.size());
        for (Size i = 0; i < pv.size(); ++i) {
            pv[i] -= 0.333333_f * (cd2v[i] - pd2v[i]) * dt2;
            pdv[i] -= 0.5_f * (cd2v[i] - pd2v[i]) * this->dt;
        }
    });
    iteratePair<VisitorEnum::FIRST_ORDER>(*this->storage, *this->predictions,
        [this](auto& pv, auto& pdv, auto& UNUSED(cv), auto& cdv) {
        ASSERT(pv.size() == pdv.size());
        for (Size i = 0; i < pv.size(); ++i) {
            pv[i] -= 0.5_f * (cdv[i] - pdv[i]) * this->dt;
        }
    });
    // clamp quantities
    /// \todo better
    for (auto& q : *this->storage) {
        q.second.clamp();
    }
    // clang-format on

    /*    iterate<VisitorEnum::HIGHEST_DERIVATIVES>(*storage, [](const QuantityIds id, auto&& dv) {
            std::cout << "Quantity " << id << std::endl;
            for (Size i = 0; i < dv.size(); ++i) {
                std::cout << i << "  |  " << dv[i] << std::endl;
            }
        });*/
    /*ArrayView<TracelessTensor> s, ds;
    tie(s, ds) = storage->getAll<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS);
    for (Size i =0;i<s.size() ;++i) {
        std::cout << i << s[i] << std::endl << ds[i] << std::endl;
    }*/
    /*ArrayView<Float> D, dD;
    tie(D, dD) = storage->getAll<Float>(QuantityIds::DAMAGE);
    for (Size i =0;i<D.size() ;++i) {
        std::cout << i << D[i] << dD[i] << std::endl;
    }*/
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// RungeKutta implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RungeKutta::RungeKutta(const std::shared_ptr<Storage>& storage, const GlobalSettings& settings)
    : Abstract::TimeStepping(storage, settings) {
    ASSERT(storage->getQuantityCnt() > 0); // quantities must already been emplaced
    k1 = std::make_unique<Storage>(storage->clone(VisitorEnum::ALL_BUFFERS));
    k2 = std::make_unique<Storage>(storage->clone(VisitorEnum::ALL_BUFFERS));
    k3 = std::make_unique<Storage>(storage->clone(VisitorEnum::ALL_BUFFERS));
    k4 = std::make_unique<Storage>(storage->clone(VisitorEnum::ALL_BUFFERS));
    storage->init(); // clear derivatives before using them in step method
}

RungeKutta::~RungeKutta() = default;

void RungeKutta::integrateAndAdvance(Abstract::Solver& solver, Storage& k, const float m, const float n) {
    solver.integrate(k);
    iteratePair<VisitorEnum::FIRST_ORDER>(k, *this->storage, [&](auto& kv, auto& kdv, auto& v, auto&) {
        for (Size i = 0; i < v.size(); ++i) {
            kv[i] += kdv[i] * m * this->dt;
            v[i] += kdv[i] * n * this->dt;
        }
    });
    iteratePair<VisitorEnum::SECOND_ORDER>(
        k, *this->storage, [&](auto& kv, auto& kdv, auto& kd2v, auto& v, auto& dv, auto&) {
            for (Size i = 0; i < v.size(); ++i) {
                kv[i] += kdv[i] * m * this->dt;
                kdv[i] += kd2v[i] * m * this->dt;
                v[i] += kdv[i] * n * this->dt;
                dv[i] += kd2v[i] * n * this->dt;
            }
        });
}

void RungeKutta::stepImpl(Abstract::Solver& solver) {
    k1->init();
    k2->init();
    k3->init();
    k4->init();

    solver.integrate(*k1);
    integrateAndAdvance(solver, *k1, 0.5_f, 1._f / 6._f);
    // swap values of 1st order quantities and both values and 1st derivatives of 2nd order quantities
    k1->swap(*k2, VisitorEnum::DEPENDENT_VALUES);

    /// \todo derivatives of storage (original, not k1, ..., k4) are never used
    // compute k2 derivatives based on values computes in previous integration
    solver.integrate(*k2);
    /// \todo at this point, I no longer need k1, we just need 2 auxiliary buffers
    integrateAndAdvance(solver, *k2, 0.5_f, 1._f / 3._f);
    k2->swap(*k3, VisitorEnum::DEPENDENT_VALUES);

    solver.integrate(*k3);
    integrateAndAdvance(solver, *k3, 0.5_f, 1._f / 3._f);
    k3->swap(*k4, VisitorEnum::DEPENDENT_VALUES);

    solver.integrate(*k4);

    iteratePair<VisitorEnum::FIRST_ORDER>(*this->storage, *k4, [this](auto& v, auto&, auto&, auto& kdv) {
        for (Size i = 0; i < v.size(); ++i) {
            v[i] += this->dt / 6.f * kdv[i];
        }
    });
    iteratePair<VisitorEnum::SECOND_ORDER>(
        *this->storage, *k4, [this](auto& v, auto& dv, auto&, auto&, auto& kdv, auto& kd2v) {
            for (Size i = 0; i < v.size(); ++i) {
                dv[i] += this->dt / 6.f * kd2v[i];
                v[i] += this->dt / 6.f * kdv[i];
            }
        });
}

NAMESPACE_SPH_END
