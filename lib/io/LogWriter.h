#pragma once

#include "objects/wrappers/SharedPtr.h"
#include "physics/Integrals.h"
#include "run/Trigger.h"

NAMESPACE_SPH_BEGIN

/// \brief Base class for objects logging run statistics.
class ILogWriter : public PeriodicTrigger {
protected:
    SharedPtr<ILogger> logger;

public:
    /// \brief Constructs the log file.
    ///
    /// This base class actually does not use the logger in any way, it is stored there (and required in the
    /// constructor) because all derived classes are expected to use a logger; this way we can reduce the code
    /// duplication.
    /// \param logger Logger for the written data. Must not be nullptr.
    /// \param period Log period in run time. Must be a positive value or zero; zero period means the log
    ///               message is written on every time step.
    explicit ILogWriter(const SharedPtr<ILogger>& logger, const Float period = 0._f);

    /// \brief Writes to the log using provided storage and statistics.
    ///
    /// Same as \ref write, implemented to allow using \ref ILogWriter as a \ref ITrigger.
    virtual AutoPtr<ITrigger> action(Storage& storage, Statistics& stats) override final;

    /// \brief Writes to the log using provided storage and statistics.
    ///
    /// Used for const-correctness (loggers should not modify storage nor stats) and returning another
    /// (non-nullptr) trigger (loggers do not create more triggers).
    virtual void write(const Storage& storage, const Statistics& stats) = 0;
};

/// \brief Writer logging useful statistics (current run time, relative progress, time step, ...).
///
/// This is the default writer used in the simulation.
class StandardLogWriter : public ILogWriter {
private:
    std::string name;

public:
    StandardLogWriter(const SharedPtr<ILogger>& logger, const RunSettings& settings);

    virtual void write(const Storage& storage, const Statistics& stats);
};

/// \brief Writer logging selected integrals of motion.
///
/// Currently fixed to logging total momentum, total angular momentum and total energy.
class IntegralsLogWriter : public ILogWriter {
private:
    TotalEnergy energy;
    TotalMomentum momentum;
    TotalAngularMomentum angularMomentum;

public:
    /// \brief Creates a writer that writes the output into given file.
    IntegralsLogWriter(const Path& path, const Size interval);

    IntegralsLogWriter(const SharedPtr<ILogger>& logger, const Size interval);

    virtual void write(const Storage& storage, const Statistics& stats) override;
};

/// \brief Helper writer that does not write any logs.
///
/// Use this to disable logging of statistics during run.
class NullLogFile : public ILogWriter {
public:
    NullLogFile();

    virtual void write(const Storage& storage, const Statistics& stats) override;
};

NAMESPACE_SPH_END
