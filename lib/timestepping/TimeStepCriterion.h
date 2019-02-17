#pragma once

/// \file TimeStepCriterion.h
/// \brief Criteria for computing the time step
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "common/ForwardDecl.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/AutoPtr.h"
#include "quantities/QuantityIds.h"

NAMESPACE_SPH_BEGIN

class IScheduler;

enum class CriterionId {
    INITIAL_VALUE, ///< Timestep is not computed, using given initial value
    MAXIMAL_VALUE, ///< Timestep given by selected maximal value
    DERIVATIVE,    ///< Timestep based on value-to-derivative ratio
    CFL_CONDITION, ///< Timestep computed using CFL condition
    ACCELERATION,  ///< Timestep constrained by acceleration condition
    MAX_CHANGE,    ///< Timestep is limited by the maximum allowed change from previous timestep
};

std::ostream& operator<<(std::ostream& stream, const CriterionId id);

struct TimeStep {
    /// Value of the time step in code units (currently SI).
    Float value;

    /// Criterion applied to compute the time step;
    CriterionId id;
};

/// \brief Base class for timestep setters.
class ITimeStepCriterion : public Polymorphic {
public:
    /// \brief Computes the value of the time step.
    ///
    /// \param scheduler Scheduler than can be used for parallelization.
    /// \param storage Storage containing all physical quantities from which the time step is determined.
    /// \param maxStep Maximal allowed time step.
    /// \param stats Used to save statistics of the criterion.
    /// \returns Computed time step and ID of criterion that determined the value.
    virtual TimeStep compute(IScheduler& scheduler, Storage& storage, Float maxStep, Statistics& stats) = 0;
};

/// \brief Criterion setting time step based on value-to-derivative ratio for time-dependent quantities.
///
/// The value-to-derivative ratio is evaluated for each particle and each first-order quantity in the storage.
/// This criterion ensures a constant relative error of the timestepping algorithms. Quantities that allows
/// for (zero or negative) values, such as all vector and tensor quantities, have to be treated differently,
/// as zero values with any non-zero derivatives would formally lead to zero time step.
/// Therefore, all time-depended quantities have a scale value assigned. For quantity value \f[x\f] with scale
/// value \f[x_0\f] and derivative \f[dx\f], the time step is computed as
/// \f[ dt = \frac{|x| + x_0}{|dx|} \f].
///
/// The scale value can be chosen arbitrarily, it represents a transitional value between relative and
/// absolute error of the quantity. The optimal scale value is about 0.01 to 0.1 of the typical value of the
/// quantity in the run; for density 2700 kg/m^3, the default scale value is 50 kg/m^3.
/// Larger scale value makes the criterion less restrictive (allowing larger time steps), and vice versa.
/// Setting the scale value to a very large number (10^20, for example) excludes the quantity from the
/// criterion.
///
/// The scale value can be set differently for each material. The value can be set by calling \ref
/// IMaterial::setRange and later obtained by IMaterial::minimal.
///
/// By default, the most restrictive time step is chosen, i.e. the lowest time step for all particles and all
/// quantities. This can be relaxed by setting a power (parameter RunSettingsId::TIMESTEPPING_MEAN_POWER) of a
/// mean, computed from time steps of individual particles. This makes the resulting time step more 'smooth',
/// as it is less influenced by outliers. The lower the value of the power, the more restrictive time step.
///
/// \note The computed mean can be quite imprecise as it uses approximative pow function. This shouldn't be an
/// issue as there is no need for exact value of time step.
///
/// \todo currently only used on first-order quantities.
class DerivativeCriterion : public ITimeStepCriterion {
private:
    /// Multiplicative factor for the time step
    Float factor;

    /// Power of the mean used to get final time step from time steps of individual particles. Value of -INFTY
    /// means the minimum of all time steps is used. Currently only implemented for negative powers.
    Float power;

public:
    explicit DerivativeCriterion(const RunSettings& settings);

    virtual TimeStep compute(IScheduler& scheduler,
        Storage& storage,
        Float maxStep,
        Statistics& stats) override;

private:
    template <template <typename> class Tls>
    TimeStep computeImpl(IScheduler& scheduler, Storage& storage, Float maxStep, Statistics& stats);
};

/// \brief Criterion settings time step based on computed acceleration of particles.
///
/// This criterion is somewhat similar to \ref DerivativeCriterion; the time step is computed from the ratio
/// of smoothing length and particle acceleration (or square root of the ratio, to be exact).
class AccelerationCriterion : public ITimeStepCriterion {
private:
    Float factor;

public:
    explicit AccelerationCriterion(const RunSettings& settings);

    virtual TimeStep compute(IScheduler& scheduler,
        Storage& storage,
        Float maxStep,
        Statistics& stats) override;
};

/// \brief Time step based on CFL criterion.
///
/// This criterion should be always used as it is necessary for stability of the integration in time. It can
/// be made more restrictive or less restrictive by setting Courant number. Setting values larger than 1 is
/// not recommended.
class CourantCriterion : public ITimeStepCriterion {
private:
    Float courant;

public:
    explicit CourantCriterion(const RunSettings& settings);

    /// Storage must contain at least positions of particles and sound speed, checked by assert.
    virtual TimeStep compute(IScheduler& scheduler,
        Storage& storage,
        Float maxStep,
        Statistics& stats) override;
};


/// \brief Helper criterion, wrapping multiple criteria under \ref ITimeStepCriterion interface.
///
/// Time step critaria can be added automatically based on parameter \ref
/// RunSettingsId::TIMESTEPPING_CRITERION in settings, or they can be specified explicitly. Each criterion
/// computes a time step and the minimal time step of these is returned.
class MultiCriterion : public ITimeStepCriterion {
private:
    Array<AutoPtr<ITimeStepCriterion>> criteria;

    /// Parameters for smoothing the timestep
    /// \todo separate into a separate criterion, doesn't belong here
    Float maxChange;
    Float lastStep;

public:
    explicit MultiCriterion(const RunSettings& settings);

    MultiCriterion(Array<AutoPtr<ITimeStepCriterion>>&& criteria, const Float maxChange, const Float initial);

    virtual TimeStep compute(IScheduler& scheduler,
        Storage& storage,
        const Float maxStep,
        Statistics& stats) override;
};

NAMESPACE_SPH_END
