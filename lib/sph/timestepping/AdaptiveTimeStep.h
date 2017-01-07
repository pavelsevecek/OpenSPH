#pragma once

#include "quantities/Storage.h"
#include "system/Profiler.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

/// Computes minimal values of array of values. Array is overwritten in the process.
/// \todo merge with ArrayStats.
Float minOfArray(Array<Float>& ar);

/// Object computing time step based on CFL condition and value-to-derivative ratio for time-dependent
/// quantities.
class AdaptiveTimeStep {
private:
    Float factor;
    Float courant;
    Array<Float> cachedSteps;

public:
    AdaptiveTimeStep(const GlobalSettings& settings);

    /// Returns the current time step.
    /// \param storage Storage containing all physical quantities from which the time step is determined.
    ///                Must contain at least positions of particles and sound speed, checked by assert.
    /// \param maxStep Maximal allowed time-step.
    /// \todo logging
    Float get(Storage& storage, const Float maxStep);

private:
    template <typename TArray>
    Float cond2ndOrder(TArray&&, TArray&&) const {
        NOT_IMPLEMENTED;
    }

    Float cond2ndOrder(LimitedArray<Vector>& v, LimitedArray<Vector>& d2v);
};

NAMESPACE_SPH_END
