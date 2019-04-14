#pragma once

#include "run/CompositeRun.h"
#include "sph/initial/Presets.h"

NAMESPACE_SPH_BEGIN

/// \brief Holds parameters of each phase.
struct PhaseParams {

    /// \brief Directory where the output files are generated.
    Path outputPath;

    struct {

        /// \brief Duration of the phase in seconds.
        ///
        /// Note that if the settings of the stabilization phase are loaded from the configuration file, this
        /// value is overriden. To preserve the set duration, set it using \ref RunSettingsId::RUN_TIME_RANGE
        /// in overrides.
        Interval range = Interval(0._f, 100._f);

        /// \brief Settings that override the default parameters.
        ///
        /// Note that these parameters also override parameters loaded from settings file.
        RunSettings overrides = EMPTY_SETTINGS;

    } stab;

    struct {

        /// \brief Duration of the phase in seconds.
        ///
        /// Note that if the settings of the fragmentation are loaded from the configuration file, this
        /// value is overriden. To preserve the set duration, set it using \ref RunSettingsId::RUN_TIME_RANGE
        /// in overrides.
        Interval range = Interval(0._f, 100._f);

        /// \brief Number of output files generated during the phase.
        Size dumpCnt = 10;

        /// \brief Settings that override the default parameters.
        ///
        /// Note that these parameters also override parameters loaded from settings file.
        RunSettings overrides = EMPTY_SETTINGS;

    } frag;

    struct {

        /// \brief Duration of the phase in seconds.
        ///
        /// Note that if the settings of the reaccumulation phase are loaded from the configuration file, this
        /// value is overriden. To preserve the set duration, set it using \ref RunSettingsId::RUN_TIME_RANGE
        /// in overrides.
        Interval range = Interval(0._f, 100._f);

        /// \brief Number of output files generated during the phase.
        Size dumpCnt = 10;

        /// \brief Settings that override the default parameters.
        ///
        /// Note that these parameters also override parameters loaded from settings file.
        RunSettings overrides = EMPTY_SETTINGS;

    } reacc;

    /// \brief If true, the durations of all phases are set to zero.
    ///
    /// Particles are set up as in real simulations, all hand-offs are performed and all configuration files
    /// are generated, but the simulation ends as soon as possible.
    bool dryRun = false;
};

class StabilizationRunPhase : public IRunPhase {
    friend class FragmentationRunPhase;

private:
    CollisionParams collisionParams;
    PhaseParams phaseParams;

    Path resumePath;

    SharedPtr<CollisionInitialConditions> collision;

public:
    /// \brief Creates a stabilization phase, given the collision setup.
    ///
    /// This is used when the stabilization is the first phase in the run.
    StabilizationRunPhase(const CollisionParams& collisionParams, const PhaseParams& phaseParams);

    /// \brief Creates a stabilization phase that continues from provided snapshot.
    StabilizationRunPhase(const Path& resumePath, const PhaseParams phaseParams);

    virtual void setUp() override;

    virtual void handoff(Storage&& input) override;

    virtual AutoPtr<IRunPhase> getNextPhase() const override;

private:
    virtual void tearDown(const Statistics& UNUSED(stats)) override {}

    void create(const PhaseParams phaseParams);
};

class FragmentationRunPhase : public IRunPhase {
    friend class ReaccumulationRunPhase;

private:
    CollisionParams collisionParams;
    PhaseParams phaseParams;

    Path resumePath;

    SharedPtr<CollisionInitialConditions> collision;

public:
    /// \brief Creates fragmentation phase that follows a stabilization phase.
    explicit FragmentationRunPhase(const StabilizationRunPhase& stabilization);

    /// \brief Creates a fragmentation phase that continues from provided snapshot.
    FragmentationRunPhase(const Path& resumePath, const PhaseParams& phaseParams);

    virtual void setUp() override;

    virtual void handoff(Storage&& input) override;

    virtual AutoPtr<IRunPhase> getNextPhase() const override;

private:
    virtual void tearDown(const Statistics& stats) override;

    void create(const PhaseParams phaseParams);
};

class ReaccumulationRunPhase : public IRunPhase {
private:
    PhaseParams phaseParams;

    Path resumePath;

public:
    /// \brief Creates reaccumulation phase that follows a fragmentation phase.
    explicit ReaccumulationRunPhase(const FragmentationRunPhase& fragmentation);

    /// \brief Creates a reaccumulation phase that continues from provided snapshot.
    ReaccumulationRunPhase(const Path& resumePath, const PhaseParams& phaseParams);

    virtual void setUp() override;

    virtual void handoff(Storage&& input) override;

    virtual AutoPtr<IRunPhase> getNextPhase() const override;

private:
    virtual void tearDown(const Statistics& stats) override;

    void create(const PhaseParams phaseParams);
};

/// \brief Simulation consisting of stabilization, fragmentation and reaccumulation phase.
class CollisionRun : public CompositeRun {
public:
    /// \brief Creates a collision simulation, given parameters of the collision.
    ///
    /// \param collisionParams Parameters specifying the initial conditions of the simulation.
    /// \param phaseParams Additional parameters of the simulation.
    /// \param runCallbacks Run callbacks used by all phases.
    explicit CollisionRun(const CollisionParams& collisionParams,
        const PhaseParams& phaseParams,
        SharedPtr<IRunCallbacks> runCallbacks);

    /// \brief Creates a simulation that continues from given snapshot.
    ///
    /// The simulation automatically selects a correct phase, i.e. when the snapshot has been saved during
    /// fragmentation phase, the \ref CollisionRun starts with fragmentation.
    /// \param path Path to the snapshot file (created with \ref BinaryOutput).
    /// \param phaseParams Additional parameters of the simulation.
    /// \param runCallbacks Run callbacks used by all phases.
    /// \throws InvalidSetup if the file cannot be loaded or has invalid format.
    explicit CollisionRun(const Path& path,
        const PhaseParams& phaseParams,
        SharedPtr<IRunCallbacks> runCallbacks);
};

NAMESPACE_SPH_END
