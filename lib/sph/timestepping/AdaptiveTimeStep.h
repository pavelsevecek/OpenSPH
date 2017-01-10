#pragma once

#include "quantities/Storage.h"
#include "system/Profiler.h"
#include "system/Settings.h"
#include "system/Statistics.h"

/// \todo forward declare stats and settings

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

    /// Returns the current time step. Value FrequentStatsIds::TIMESTEP_CRITERION of statistics is set to
    /// condition that limits the value of the timestep.
    /// \param storage Storage containing all physical quantities from which the time step is determined.
    ///                Must contain at least positions of particles and sound speed, checked by assert.
    /// \param maxStep Maximal allowed time-step.
    Float get(Storage& storage, const Float maxStep, FrequentStats& stats);

private:
    template <typename TArray>
    Float cond2ndOrder(TArray&&, TArray&&) const {
        NOT_IMPLEMENTED;
    }

    Float cond2ndOrder(Array<Vector>& v, Array<Vector>& d2v);
};

NAMESPACE_SPH_END
