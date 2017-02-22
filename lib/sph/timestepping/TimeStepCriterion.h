#pragma once

#include "objects/ExtendEnum.h"
#include "objects/ForwardDecl.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/Value.h"
#include "quantities/QuantityIds.h"

NAMESPACE_SPH_BEGIN

enum class CriterionIds {
    INITIAL_VALUE = 100, ///< Timestep is not computed, using given initial value
    MAXIMAL_VALUE = 101, ///< Timestep is given by selected maximal value
    CFL_CONDITION = 102, ///< Timestep is computed using CFL condition
    ACCELERATION = 103   ///< Timestep is constrained by acceleration condition
};
using AllCriterionIds = ExtendEnum<CriterionIds, QuantityIds>;

/// \todo remove AllCriterionIds, instead use CriterionIds::DERIVATIVE and save relevant quantity to
/// Statistics
template <typename TStream>
TStream& operator<<(TStream& stream, AllCriterionIds id) {
    switch ((CriterionIds)id) {
    case CriterionIds::CFL_CONDITION:
        stream << "CFL condition";
        break;
    case CriterionIds::ACCELERATION:
        stream << "Acceleration";
        break;
    case CriterionIds::MAXIMAL_VALUE:
        stream << "Maximal value";
        break;
    case CriterionIds::INITIAL_VALUE:
        stream << "Default value";
        break;
    default:
        stream << getQuantityName(id);
    }
    return stream;
}

namespace Abstract {

    /// Base class for timestep setters.
    class TimeStepCriterion : public Polymorphic {
    public:
        /// Returns the current time step.
        /// \param storage Storage containing all physical quantities from which the time step is determined.
        /// \param maxStep Maximal allowed time step.
        /// \param stats Optional parameter used to save statistics of the criterion.
        /// \returns Tuple, containing computed time step and ID of quantity that determined the value.
        virtual Tuple<Float, AllCriterionIds> compute(Storage& storage,
            const Float maxStep,
            Optional<Statistics&> stats = NOTHING) = 0;
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

    virtual Tuple<Float, AllCriterionIds> compute(Storage& storage,
        const Float maxStep,
        Optional<Statistics&> stats = NOTHING) override;
};

/// Criterion settings time step based on computed acceleration of particles.
class AccelerationCriterion : public Abstract::TimeStepCriterion {
public:
    virtual Tuple<Float, AllCriterionIds> compute(Storage& storage,
        const Float maxStep,
        Optional<Statistics&> stats = NOTHING) override;
};

/// Time step based on CFL criterion.
class CourantCriterion : public Abstract::TimeStepCriterion {
private:
    Float courant;

public:
    CourantCriterion(const GlobalSettings& settings);

    /// Storage must contain at least positions of particles and sound speed, checked by assert.
    virtual Tuple<Float, AllCriterionIds> compute(Storage& storage,
        const Float maxStep,
        Optional<Statistics&> stats = NOTHING) override;
};


/// Helper "virtual" criterion, wrapping multiple criteria under Abstract::AdaptiveTimeStep interface.
class MultiCriterion : public Abstract::TimeStepCriterion {
private:
    StaticArray<std::unique_ptr<Abstract::TimeStepCriterion>, 3> criteria;

public:
    MultiCriterion(const GlobalSettings& settings);

    virtual Tuple<Float, AllCriterionIds> compute(Storage& storage,
        const Float maxStep,
        Optional<Statistics&> stats = NOTHING) override;
};

NAMESPACE_SPH_END
