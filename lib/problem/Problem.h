#pragma once

#include "common/ForwardDecl.h"
#include "objects/wrappers/Range.h"
#include "physics/Integrals.h"
#include "system/Settings.h"
#include <memory>

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Output;
    class Callbacks;
    class LogFile;
    class Solver;
}

/// Defines the interface for a run. Each problem must implement methods \ref setUp and \ref tearDown,
/// settings up initial conditions for the run and closing down the run, respectivelly.
///
/// Implementation can set up all member variables to any given value. If any variable is left uninitialized,
/// it will be initialized to a default value as specified by global settings. Only particle storage MUST be
/// initialized by the setUp function.
namespace Abstract {
    class Problem : public Noncopyable {
    protected:
        GlobalSettings settings;

        /// Data output
        std::unique_ptr<Abstract::Output> output;

        /// GUI callbacks
        std::unique_ptr<Abstract::Callbacks> callbacks;

        /// Logging
        std::shared_ptr<Abstract::Logger> logger;

        /// Stores all SPH particles
        std::shared_ptr<Storage> storage;

        /// Timestepping
        std::unique_ptr<Abstract::TimeStepping> timeStepping;

        /// Solver
        std::unique_ptr<Abstract::Solver> solver;

        /// Logging files
        Array<std::unique_ptr<Abstract::LogFile>> logFiles;

    public:
        Problem();

        ~Problem();

        /// Starts the run
        void run();

    protected:
        /// Prepares the run, sets all initial conditions, creates logger, output, ...
        virtual void setUp() = 0;

        /// Called after the run, saves all necessary data, logs run statistics, ...
        virtual void tearDown() = 0;

    private:
        void setNullToDefaults();
    };
}

NAMESPACE_SPH_END
