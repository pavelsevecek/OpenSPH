#pragma once

/// \file TimeStepping.h
/// \brief Algorithms for temporal evolution of the physical model.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/ForwardDecl.h"
#include "objects/containers/Array.h"
#include "objects/geometry/Vector.h"
#include "objects/wrappers/SharedPtr.h"

NAMESPACE_SPH_BEGIN

class IScheduler;

/// \brief Base object providing integration in time for all quantities.
///
/// The integration is done by iterating with discrete time step, using \ref step method. All derived
/// objects must implement \ref stepImpl function, which shall iterate over all independant quantities and
/// advance their values using temporal derivatives computed by \ref ISolver object passed in argument
/// of the method. The \ref step function then calls the user-defined \ref stepImpl while also doing more
/// legwork, such as saving statistics and computing new value of time step. Function \ref stepImpl can
/// also save statistics specific to the implementation, using provided \ref Statistics object, but it shall
/// not compute the time step value. To control time step, see \ref TimeStepCriterion and derived classes.
///
/// The time-stepping object must take care of clearing derivatives, as there can be values from previous
/// timestep, or some garbage memory when the method is called for the first time. It is also necessary to
/// clamp all quantities by their minimal/maximal allowed values. These values can be different for
/// different materials; the range of quantity for given \ref IMaterial object can be obtained by
/// \ref IMaterial::range() const function.
///
/// Temporal derivatives are computed by calling \ref ISolver::integrate function. The solver
/// assumes the storage already has zeroed highest-order derivatives of all quantities. The implementation of
/// \ref ITimeStepping inteface must also assume that \ref ISolver::integrate changes the number of particles
/// in the storage. If the timestepping uses derivatives from different times, stored in separate Storage
/// objects, the variable number of particles can be handled automatically by the Storage object, using the
/// concept of dependenet storages - if you create an auxiliary storage, for example to store the prediction
/// derivatives in predictor-corrector algorithm, simply add this storage as a dependent storage to the main
/// one, and it will be updated every time the number of particles in the main storage changes.
///
/// Timestepping is bound to a single Storage object, passed in constructor, and this object cannot be changed
/// during the lifetime of the timestepping object. Timestepping implementation can create any number of
/// auxiliary storages and link then to the main one, this hierarchy established in the constructor will not
/// change during the run.
class ITimeStepping : public Polymorphic {
protected:
    /// Main storage holding all the particles in the run
    SharedPtr<Storage> storage;

    /// Current time step
    Float timeStep;

    /// Maximal allowed time step
    Float maxTimeStep;

    /// Criterion used to compute the time step
    AutoPtr<ITimeStepCriterion> criterion;

public:
    /// \brief Constructs the timestepping, using timestep criteria from parameters in settings.
    ///
    /// \param storage Storage used during the run
    /// \param settings Settings containing initial and maximal timestep and aslo timestep criteria
    ITimeStepping(const SharedPtr<Storage>& storage, const RunSettings& settings);

    /// \brief Constructs the timestepping, explicitly specifying the timestep criterion used in the run.
    ///
    /// \note Use MultiCriterion if more than one criterion is used.
    ITimeStepping(const SharedPtr<Storage>& storage,
        const RunSettings& settings,
        AutoPtr<ITimeStepCriterion>&& criterion);

    ~ITimeStepping() override;

    INLINE Float getTimeStep() const {
        return timeStep;
    }

    void step(IScheduler& scheduler, ISolver& solver, Statistics& stats);

protected:
    virtual void stepImpl(IScheduler& scheduler, ISolver& solver, Statistics& stats) = 0;
};


/// \brief Simple Euler first-order timestepping.
class EulerExplicit : public ITimeStepping {
public:
    explicit EulerExplicit(const SharedPtr<Storage>& storage, const RunSettings& settings)
        : ITimeStepping(storage, settings) {}

    virtual void stepImpl(IScheduler& scheduler, ISolver& solver, Statistics& stats) override;
};

/// \brief Predictor-corrector second-order timestepping
class PredictorCorrector : public ITimeStepping {
private:
    /// Separate storage holding prediction derivatives. Holds only highest-order derivatives, other buffers
    /// are empty. Must be kept synchronized with the main storage.
    SharedPtr<Storage> predictions;

public:
    PredictorCorrector(const SharedPtr<Storage>& storage, const RunSettings& settings);

    ~PredictorCorrector() override;

protected:
    virtual void stepImpl(IScheduler& scheduler, ISolver& solver, Statistics& stats) override;

    void makePredictions(IScheduler& scheduler);

    void makeCorrections(IScheduler& scheduler);
};

/// \brief Leapfrog timestepping
///
/// Uses the drift-kick-drift version of the algorithm for second-order quantities. First-order quantities are
/// integrated using ordinary Euler timestepping.
class LeapFrog : public ITimeStepping {
public:
    LeapFrog(const SharedPtr<Storage>& storage, const RunSettings& settings)
        : ITimeStepping(storage, settings) {}

protected:
    virtual void stepImpl(IScheduler& scheduler, ISolver& solver, Statistics& stats) override;
};

class RungeKutta : public ITimeStepping {
private:
    SharedPtr<Storage> k1, k2, k3, k4;

public:
    RungeKutta(const SharedPtr<Storage>& storage, const RunSettings& settings);

    ~RungeKutta() override;

protected:
    virtual void stepImpl(IScheduler& scheduler, ISolver& solver, Statistics& stats) override;

    void integrateAndAdvance(ISolver& solver, Statistics& stats, Storage& k, const Float m, const Float n);
};


class ModifiedMidpointMethod : public ITimeStepping {
private:
    SharedPtr<Storage> mid;
    Size n;

public:
    ModifiedMidpointMethod(const SharedPtr<Storage>& storage, const RunSettings& settings);

protected:
    virtual void stepImpl(IScheduler& scheduler, ISolver& solver, Statistics& stats) override;
};

class BulirschStoer : public ITimeStepping {
private:
    Float eps;

public:
    BulirschStoer(const SharedPtr<Storage>& storage, const RunSettings& settings);

protected:
    virtual void stepImpl(IScheduler& scheduler, ISolver& solver, Statistics& stats) override;
};

NAMESPACE_SPH_END
