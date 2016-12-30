#include "sph/timestepping/TimeStepping.h"
#include "solvers/AbstractSolver.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

void EulerExplicit::stepImpl(Abstract::Solver& solver) {
    MEASURE_SCOPE("EulerExplicit::step");
    // clear derivatives from previous timestep
    this->storage->init();

    // compute derivatives
    solver.integrate(*this->storage);

    PROFILE_SCOPE("EulerExplicit::step")
    // advance all 2nd-order quantities by current timestep, first values, then 1st derivatives
    iterate<VisitorEnum::SECOND_ORDER>(*this->storage, [this](auto& v, auto& dv, auto& d2v) {
        for (int i = 0; i < v.size(); ++i) {
            dv[i] += d2v[i] * this->dt;
            v[i] += dv[i] * this->dt;
            v.clamp(i);
        }
    });
    iterate<VisitorEnum::FIRST_ORDER>(*this->storage, [this](auto& v, auto& dv) {
        for (int i = 0; i < v.size(); ++i) {
            v[i] += dv[i] * this->dt;
            v.clamp(i);
        }
    });
}


PredictorCorrector::PredictorCorrector(const std::shared_ptr<Storage>& storage,
    const GlobalSettings& settings)
    : Abstract::TimeStepping(storage, settings) {
    ASSERT(storage->getQuantityCnt() > 0); // quantities must already been emplaced
    predictions = storage->clone(VisitorEnum::HIGHEST_DERIVATIVES);
    storage->init(); // clear derivatives before using them in step method
}

void PredictorCorrector::stepImpl(Abstract::Solver& solver) {
    const Float dt2 = 0.5_f * Math::sqr(this->dt);

    PROFILE_SCOPE("PredictorCorrector::step   Predictions")
    // make prediction using old derivatives (simple euler)
    iterate<VisitorEnum::SECOND_ORDER>(*this->storage, [this, dt2](auto& v, auto& dv, auto& d2v) {
        for (int i = 0; i < v.size(); ++i) {
            v[i] += dv[i] * this->dt + d2v[i] * dt2;
            dv[i] += d2v[i] * this->dt;
            v.clamp(i);
        }
    });
    iterate<VisitorEnum::FIRST_ORDER>(*this->storage, [this](auto& v, auto& dv) {
        for (int i = 0; i < v.size(); ++i) {
            v[i] += dv[i] * this->dt;
            v.clamp(i);
        }
    });
    // save derivatives from predictions
    this->storage->swap(predictions, VisitorEnum::HIGHEST_DERIVATIVES);

    // clear derivatives
    this->storage->init();
    SCOPE_STOP
    // compute derivative
    solver.integrate(*this->storage);
    PROFILE_NEXT("PredictorCorrector::step   Corrections");
    // make corrections
    // clang-format off
    iteratePair<VisitorEnum::SECOND_ORDER>(*this->storage, this->predictions,
        [this, dt2](auto& pv, auto& pdv, auto& pd2v, auto& UNUSED(cv), auto& UNUSED(cdv), auto& cd2v) {
        ASSERT(pv.size() == pd2v.size());
        for (int i = 0; i < pv.size(); ++i) {
            pv[i] -= 0.333333_f * (cd2v[i] - pd2v[i]) * dt2;
            pdv[i] -= 0.5_f * (cd2v[i] - pd2v[i]) * this->dt;
            pv.clamp(i);
        }
    });
    iteratePair<VisitorEnum::FIRST_ORDER>(*this->storage, this->predictions,
        [this](auto& pv, auto& pdv, auto& UNUSED(cv), auto& cdv) {
        ASSERT(pv.size() == pdv.size());
        for (int i = 0; i < pv.size(); ++i) {
            pv[i] -= 0.5_f * (cdv[i] - pdv[i]) * this->dt;
            pv.clamp(i);
        }
    });
    // clang-format on
}

NAMESPACE_SPH_END
