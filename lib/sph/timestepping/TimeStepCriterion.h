#pragma once

#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

// forward decl

class Storage;
enum class QuantityIds;

template <typename TEnum>
class Settings;
enum class GlobalSettingsIds;

using GlobalSettings = Settings<GlobalSettingsIds>;


namespace Abstract {

    /// Base class for timestep setters.
    class TimeStepCriterion : public Polymorphic {
    public:
        /// Returns the current time step.
        /// \param storage Storage containing all physical quantities from which the time step is determined.
        /// \param maxStep Maximal allowed time step.
        /// \returns Tuple, containing computed time step and ID of quantity that determined the value.
        virtual Tuple<Float, QuantityIds> compute(Storage& storage, const Float maxStep) = 0;
    };
}

/// Criterion setting time step based on value-to-derivative ratio for time-dependent quantities.
/// \todo add variability, currently sets timestep by mininum of all quantities and all particles, that may be
/// too strict and limiting (one outlier will set timestep for all).
/// \todo currently only used on first-order quantities.
class DerivativeCriterion : public Abstract::TimeStepCriterion {
private:
    Float factor;

public:
    DerivativeCriterion(const GlobalSettings& settings);

    virtual Tuple<Float, QuantityIds> compute(Storage& storage, const Float maxStep) override;
};

/// Criterion settings time step based on computed acceleration of particles.
class AccelerationCriterion : public Abstract::TimeStepCriterion {
public:
    virtual Tuple<Float, QuantityIds> compute(Storage& storage, const Float maxStep) override;
};

/// Time step based on CFL criterion.
class CourantCriterion : public Abstract::TimeStepCriterion {
private:
    Float courant;

public:
    CourantCriterion(const GlobalSettings& settings);

    /// Storage must contain at least positions of particles and sound speed, checked by assert.
    virtual Tuple<Float, QuantityIds> compute(Storage& storage, const Float maxStep) override;
};


/// Helper "virtual" criterion, wrapping multiple criteria under Abstract::AdaptiveTimeStep interface.
class MultiCriterion : public Abstract::TimeStepCriterion {
private:
    StaticArray<std::unique_ptr<Abstract::TimeStepCriterion>, 3> criteria;

public:
    MultiCriterion(const GlobalSettings& settings);

    virtual Tuple<Float, QuantityIds> compute(Storage& storage, const Float maxStep) override;
};

NAMESPACE_SPH_END
