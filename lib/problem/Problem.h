#pragma once

#include "objects/containers/Tuple.h"
#include "objects/finders/Finder.h"
#include "objects/wrappers/Range.h"
#include "sph/distributions/Distribution.h"
#include "sph/kernel/Kernel.h"
#include "sph/timestepping/TimeStepping.h"
#include "storage/QuantityKey.h"
#include "sph/boundary/Boundary.h"
#include "system/Callbacks.h"
#include "system/Logger.h"
#include "system/Output.h"
#include "solvers/ContinuitySolver.h"


NAMESPACE_SPH_BEGIN


/// class DomainEnforce ?
///  initial distribution of particles ~ domain
///  nothing
///  ghost particles
///  hard domain (setting positions without interactions)
///  periodic conditions


template <typename TModel>
class Problem : public Noncopyable {
public:
    /// Selected computational domain
    std::unique_ptr<Abstract::Domain> domain;

    /// Logging
    std::unique_ptr<Abstract::Logger> logger;

    /// Data output
    std::unique_ptr<Abstract::Output> output;

    /// GUI callbacks
    std::unique_ptr<Abstract::Callbacks> callbacks;

    /// Timestepping
    std::unique_ptr<Abstract::TimeStepping> timeStepping;

    std::unique_ptr<Abstract::BoundaryConditions> boundary;

    /// Time range of the simulations
    /// \todo other conditions? For example pressure-limited simulations?
    Range timeRange;

    /// Stores all SPH particles
    std::shared_ptr<Storage> storage;

    /// Implements computations of quantities and their temporal evolution
    TModel model;


    /// initialize problem by constructing model
    template <typename... TArgs>
    Problem(TArgs&&... args)
        : storage(std::make_shared<Storage>())
        , model(storage, std::forward<TArgs>(args)...) {}

    void run() {
        int i = 0;

        /// \todo don't use global settings ...
        const int outputEvery = GLOBAL_SETTINGS.get<int>(GlobalSettingsIds::RUN_OUTPUT_STEP);

        for (Float& t : rangeAdapter(timeRange, timeStepping->getTimeStep())) {
            const Float dt = timeStepping->getTimeStep();
            t += dt;

            // Log
            if (logger) {
                logger->write("t = " + std::to_string(t) + ", dt = " + std::to_string(dt));
            }

            // Dump output
            if (output && (i % outputEvery == 0)) {
                output->dump(*storage, t);
            }
            i++;

            // Make time step
            timeStepping->step(&model);

            if (callbacks) {
                callbacks->onTimeStep(storage->get<QuantityKey::R>());
            }
        }
    }
};

NAMESPACE_SPH_END
