#pragma once

#include "objects/finders/Finder.h"
#include "objects/wrappers/Range.h"
#include "objects/containers/Tuple.h"
#include "physics/TimeStepping.h"
#include "sph/initconds/InitConds.h"
#include "sph/kernel/Kernel.h"
#include "storage/BasicView.h"
#include "system/Callbacks.h"
#include "system/Logger.h"


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

    /// Output
    std::unique_ptr<Abstract::Logger> logger;

    /// GUI callbacks
    std::unique_ptr<Abstract::Callbacks> callbacks;

    /// Time range of the simulations
    /// \todo other conditions? For example pressure-limited simulations?
    Range<Float> timeRange;

    /// Stores all SPH particles
    std::shared_ptr<GenericStorage> storage;

    /// Implements computations of quantities and their temporal evolution
    TModel model;

    /// "View" of particles \todo needed?
    std::unique_ptr<typename TModel::View> view;


    /// initialize problem by constructing model
    template <typename... TArgs>
    Problem(TArgs&&... args)
        : model(std::forward<TArgs>(args)...)
        , storage(std::make_shared<GenericStorage>())
        , view(storage->makeViewer<typename TModel::View>()) {}

    void init(const int n, Abstract::Distribution* distribution) {
        /// Generate initial conditions
        model.create(n, distribution, domain.get());
        /// \todo by default smoothing lengths h are generated as for eta = 1, account for different values
    }

    void run() {
        Float t = 0._f;
        while (true) {
            // for (Float& t : rangeAdapter(timeRange, timestepping->getTimeStep())) {
            /// Output

            // logger->write("Time: " + std::to_string(t) + " s");
            // t += timestepping->getTimeStep();
            /// Initialize all quantities
            // model.init();

            // model.makeTimeStep(timestepping.get());

            if (callbacks) {
                callbacks->onTimeStep(view->rs);
            }
        }
    }
};

NAMESPACE_SPH_END
