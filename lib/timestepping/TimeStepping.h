#pragma once

/// \file TimeStepping.h
/// \brief Algorithms for temporal evolution of the physical model.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/ForwardDecl.h"
#include "objects/containers/Array.h"
#include "objects/geometry/Vector.h"
#include "objects/wrappers/SharedPtr.h"

NAMESPACE_SPH_BEGIN


/// \brief Base object providing integration in time for all quantities.
///
/// The integration is done by iterating with discrete time step, using \ref step method. All derived
/// objects must implement \ref stepImpl function, which iterates over all independant quantities and
/// advances their values using temporal derivatives computed by \ref ISolver object passed in argument
/// of the method. The \ref step function then calls the user-defined \ref stepImpl while also doing more
/// legwork, such as saving statistics and computing new value of time step. Function \ref stepImpl can
/// also save statistics specific to the implementation to provided \ref Statistics object, but shall not
/// compute the time step value. To control time step, see \ref TimeStepCriterion and derived classes.
///
/// The time-stepping object must take care of clearing derivatives, as there can be values from previous
/// timestep, or some garbage memory when the method is called for the first time. It is also necessary to
/// clamp all quantities by their minimal/maximal allowed values. These values can be different for
/// different materials; the range of quantity for given \ref IMaterial object can be obtained by
/// \ref IMaterial::range() const function.
///
/// Temporal derivatives are computed by calling \ref ISolver::integrate function. The solver
/// assumes the storage already has zeroed highest-order derivatives of all quantities. The implementation of
/// \ref ITimeStepping inteface must assume that \ref ISolver::integrate changes the number of particles in
/// the storage.
///
/// \todo We have currently no way of matching the particles before and after \ref ISolver::integrate has been
/// called. This wull screw up timestepping algorithms keeping a copy of the derivatives, such as \ref
/// PredictorCorrector. It is therefore safe only to add or remove particles from the back of the storage. It
/// will be necessary to mark the particles as deleted rather than actually deleting them, and let the
/// timestepping implementation delete them from the storage.
/// OR we could include an extra mapping quantity, simply containing indices from 0 to particleCnt-1, so if
/// some particles were removed or shuffled, we would know the indices of the original particles.
///
class ITimeStepping : public Polymorphic {
protected:
    SharedPtr<Storage> storage;
    Float dt;
    Float maxdt;
    AutoPtr<ITimeStepCriterion> adaptiveStep;

public:
    ITimeStepping(const SharedPtr<Storage>& storage, const RunSettings& settings);

    virtual ~ITimeStepping();

    INLINE Float getTimeStep() const {
        return dt;
    }

    void step(ISolver& solver, Statistics& stats);

protected:
    virtual void stepImpl(ISolver& solver, Statistics& stats) = 0;
};


/// \brief Simple Euler first-order timestepping.
class EulerExplicit : public ITimeStepping {
public:
    explicit EulerExplicit(const SharedPtr<Storage>& storage, const RunSettings& settings)
        : ITimeStepping(storage, settings) {}

    virtual void stepImpl(ISolver& solver, Statistics& stats) override;
};

/// \brief Predictor-corrector second-order timestepping
class PredictorCorrector : public ITimeStepping {
private:
    /// Separate storage holding prediction derivatives. Holds only highest-order derivatives, other buffers
    /// are empty. Must be kept synchronized with the main storage.
    AutoPtr<Storage> predictions;

public:
    PredictorCorrector(const SharedPtr<Storage>& storage, const RunSettings& settings);

    ~PredictorCorrector();

protected:
    virtual void stepImpl(ISolver& solver, Statistics& stats) override;

    void makePredictions();

    void makeCorrections();
};

class LeapFrog : public ITimeStepping {
public:
    LeapFrog(const SharedPtr<Storage>& storage, const RunSettings& settings)
        : ITimeStepping(storage, settings) {}

protected:
    virtual void stepImpl(ISolver& UNUSED(solver), Statistics& UNUSED(stats)) override {
        NOT_IMPLEMENTED;
    }
};

class RungeKutta : public ITimeStepping {
private:
    AutoPtr<Storage> k1, k2, k3, k4;

public:
    RungeKutta(const SharedPtr<Storage>& storage, const RunSettings& settings);

    ~RungeKutta();

protected:
    virtual void stepImpl(ISolver& solver, Statistics& stats) override;

    void integrateAndAdvance(ISolver& solver, Statistics& stats, Storage& k, const float m, const float n);
};


class BulirschStoer : public ITimeStepping {
public:
    BulirschStoer(const SharedPtr<Storage>& storage, const RunSettings& settings)
        : ITimeStepping(storage, settings) {}

protected:
    virtual void stepImpl(ISolver& UNUSED(solver), Statistics& UNUSED(stats)) override {
        NOT_IMPLEMENTED
    }
};

NAMESPACE_SPH_END
