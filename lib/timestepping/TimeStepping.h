#pragma once

/// \file TimeStepping.h
/// \brief Algorithms for temporal evolution of the physical model.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/ForwardDecl.h"
#include "objects/geometry/Vector.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/SharedPtr.h"

NAMESPACE_SPH_BEGIN


namespace Abstract {
    /// \brief Base object providing integration in time for all quantities.
    ///
    /// The integration is done by iterating with discrete time step, using \ref step method. All derived
    /// objects must implement \ref stepImpl function, which iterates over all independant quantities and
    /// advances their values using temporal derivatives computed by \ref Abstract::Model passed in argument
    /// of the method. The \ref step function then calls the user-defined \ref stepImpl while also doing more
    /// legwork, such as saving statistics and computing new value of time step. Function \ref stepImpl can
    /// also save statistics specific to the implementation to provided \ref Statistics object, but shall not
    /// compute the time step value. To control time step, see \ref TimeStepCriterion and derived classes.
    ///
    /// The time-stepping object must take care of clearing derivatives, as there can be values from previous
    /// timestep, or some garbage memory when the method is called for the first time. It is also necessary to
    /// clamp all quantities by their minimal/maximal allowed values. These values can be different for
    /// different materials; the range of quantity for given \ref Abstract::Material object can be obtained by
    /// \ref Abstract::Material::range() const function.
    ///
    /// Temporal derivatives are computed by calling \ref Abstract::Solver::integrate function. The solver
    /// assumes the storage already has zeroed highest-order derivatives of all quantities.
    class TimeStepping : public Polymorphic {
    protected:
        SharedPtr<Storage> storage;
        Float dt;
        Float maxdt;
        AutoPtr<Abstract::TimeStepCriterion> adaptiveStep;

    public:
        TimeStepping(const SharedPtr<Storage>& storage, const RunSettings& settings);

        virtual ~TimeStepping();

        INLINE Float getTimeStep() const {
            return dt;
        }

        void step(Abstract::Solver& solver, Statistics& stats);

    protected:
        virtual void stepImpl(Abstract::Solver& solver, Statistics& stats) = 0;
    };
}

/// Simple Euler first-order timestepping.
class EulerExplicit : public Abstract::TimeStepping {
public:
    explicit EulerExplicit(const SharedPtr<Storage>& storage, const RunSettings& settings)
        : Abstract::TimeStepping(storage, settings) {}

    virtual void stepImpl(Abstract::Solver& solver, Statistics& stats) override;
};

/// Predictor-corrector second-order timestepping
class PredictorCorrector : public Abstract::TimeStepping {
private:
    /// Separate storage holding prediction derivatives. Holds only highest-order derivatives, other buffers
    /// are empty. Must be kept synchronized with the main storage.
    AutoPtr<Storage> predictions;

public:
    PredictorCorrector(const SharedPtr<Storage>& storage, const RunSettings& settings);

    ~PredictorCorrector();

protected:
    virtual void stepImpl(Abstract::Solver& solver, Statistics& stats) override;

    void makePredictions();

    void makeCorrections();
};

class LeapFrog : public Abstract::TimeStepping {
public:
    LeapFrog(const SharedPtr<Storage>& storage, const RunSettings& settings)
        : Abstract::TimeStepping(storage, settings) {}

protected:
    virtual void stepImpl(Abstract::Solver& UNUSED(solver), Statistics& UNUSED(stats)) override {
        NOT_IMPLEMENTED;
    }
};

class RungeKutta : public Abstract::TimeStepping {
private:
    AutoPtr<Storage> k1, k2, k3, k4;

public:
    RungeKutta(const SharedPtr<Storage>& storage, const RunSettings& settings);

    ~RungeKutta();

protected:
    virtual void stepImpl(Abstract::Solver& solver, Statistics& stats) override;

    void integrateAndAdvance(Abstract::Solver& solver,
        Statistics& stats,
        Storage& k,
        const float m,
        const float n);
};


class BulirschStoer : public Abstract::TimeStepping {
public:
    BulirschStoer(const SharedPtr<Storage>& storage, const RunSettings& settings)
        : Abstract::TimeStepping(storage, settings) {}

protected:
    virtual void stepImpl(Abstract::Solver& UNUSED(solver), Statistics& UNUSED(stats)) override {
        NOT_IMPLEMENTED
    }
};

NAMESPACE_SPH_END
