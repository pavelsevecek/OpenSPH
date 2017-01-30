#pragma once

/// Algorithms for temporal evolution of the physical model.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Vector.h"
#include "objects/ForwardDecl.h"
#include "objects/containers/Array.h"
#include <memory>

NAMESPACE_SPH_BEGIN

/// Base object providing integration in time for all quantities.
///
/// The integration is done by iterating with discrete time step, using step() method. All derived objects
/// must implement step() function, which iterates over all independant quantities and advances their values
/// using temporal derivatives computed by Abstract::Model passed in argument of the method.
///
/// The time-stepping object must take care of clearing derivatives, as there can be values from previous
/// timestep, or some garbage memory when the method is called for the first time. It is also necessary to
/// clamp all quantities by their minimal/maximal allowed values.
///
/// When solver->compute is called, the storage passed as an argument MUST have zero highest order
/// derivatives.
namespace Abstract {
    class TimeStepping : public Polymorphic {
    protected:
        std::shared_ptr<Storage> storage;
        Float dt;
        Float maxdt;
        std::unique_ptr<Abstract::TimeStepCriterion> adaptiveStep;

    public:
        TimeStepping(const std::shared_ptr<Storage>& storage, const GlobalSettings& settings);

        virtual ~TimeStepping();

        INLINE Float getTimeStep() const { return dt; }

        void step(Abstract::Solver& solver, Statistics& stats);

    protected:
        virtual void stepImpl(Abstract::Solver& solver) = 0;
    };
}


class EulerExplicit : public Abstract::TimeStepping {
public:
    explicit EulerExplicit(const std::shared_ptr<Storage>& storage, const GlobalSettings& settings)
        : Abstract::TimeStepping(storage, settings) {}

    virtual void stepImpl(Abstract::Solver& solver) override;
};


class PredictorCorrector : public Abstract::TimeStepping {
private:
    std::unique_ptr<Storage> predictions;

public:
    PredictorCorrector(const std::shared_ptr<Storage>& storage, const GlobalSettings& settings);

    ~PredictorCorrector();

protected:
    virtual void stepImpl(Abstract::Solver& solver) override;
};

class LeapFrog : public Abstract::TimeStepping {
public:
    LeapFrog(const std::shared_ptr<Storage>& storage, const GlobalSettings& settings)
        : Abstract::TimeStepping(storage, settings) {}

protected:
    virtual void stepImpl(Abstract::Solver& UNUSED(solver)) override {}
};

class RungeKutta : public Abstract::TimeStepping {
private:
    std::unique_ptr<Storage> k1, k2, k3, k4;

public:
    RungeKutta(const std::shared_ptr<Storage>& storage, const GlobalSettings& settings);

    ~RungeKutta();

protected:
    virtual void stepImpl(Abstract::Solver& solver) override;

    void integrateAndAdvance(Abstract::Solver& solver, Storage& k, const float m, const float n);
};


class BulirschStoer : public Abstract::TimeStepping {
public:
    BulirschStoer(const std::shared_ptr<Storage>& storage, const GlobalSettings& settings)
        : Abstract::TimeStepping(storage, settings) {}

protected:
    virtual void stepImpl(Abstract::Solver& UNUSED(solver)) override {}
};

NAMESPACE_SPH_END
