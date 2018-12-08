#pragma once

/// \file CompositeRun.h
/// \brief Simulation composed of multiple phases with generally different solvers
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "run/IRun.h"
#include "run/RunCallbacks.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

class IRunPhase : public IRun {
    friend class CompositeRun;

public:
    /// \brief Performs a hand-off, taking a results of previous phase in storage.
    ///
    /// This is called instead of \ref setUp for all run phases except for the first one.
    virtual void handoff(Storage&& input) = 0;

    /// \brief Returns the next phase, following this run.
    ///
    /// If nullptr, this is the last phase of the composite run.
    virtual AutoPtr<IRunPhase> getNextPhase() const = 0;
};

enum class RunTypeEnum;

class CompositeRun : public IRun {
protected:
    /// \brief First phase to be run, following phases are obtained using \ref IRunPhase::getNextPhase.
    SharedPtr<IRunPhase> first;

    /// \brief Generic callback executed after each phase.
    Function<void(const IRunPhase&)> onNextPhase = nullptr;

public:
    CompositeRun() = default;

    CompositeRun(SharedPtr<IRunPhase> first, Function<void(const IRunPhase&)> onNextPhase)
        : first(std::move(first))
        , onNextPhase(onNextPhase) {}

    virtual void setUp() override {
        ASSERT(first);
        first->setUp();
        storage = first->getStorage();

        if (callbacks) {
            // override the callbacks by shared value
            first->callbacks = callbacks;
        }
    }

    virtual void run() override {
        SharedPtr<IRunPhase> current = first;

        while (true) {
            // run the current phase
            current->run();

            // check whether another phase follows
            SharedPtr<IRunPhase> next = current->getNextPhase();
            if (!next) {
                break;
            }

            // make the handoff, using the storage of the previous run
            try {
                next->handoff(std::move(*storage));
            } catch (std::exception&) {
                return;
            }

            // copy the callbacks
            next->callbacks = callbacks;

            storage = next->getStorage();

            if (onNextPhase) {
                onNextPhase(*next);
            }

            current = next;
        }
    }

protected:
    virtual void tearDown(const Statistics& UNUSED(stats)) override {}
};

NAMESPACE_SPH_END
