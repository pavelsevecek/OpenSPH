#pragma once

/// \file CompositeRun.h
/// \brief Simulation composed of multiple phases with generally different solvers
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "run/IRun.h"

NAMESPACE_SPH_BEGIN

class IRunPhase : public IRun {
    friend class CompositeRun;

public:
    /// \brief Returns the next phase, following this run.
    ///
    /// If nullptr, this is the last phase of the composite run.
    virtual AutoPtr<IRunPhase> getNextPhase() const = 0;

    /// \brief Performs a hand-off, taking a results of previous phase in storage.
    ///
    /// This is called instead of \ref setUp for all run phases except for the first one.
    virtual void handoff(Storage&& input) = 0;
};

class CompositeRun : public IRun {
private:
    AutoPtr<IRunPhase> first;
    Function<void(const Storage& storage)> onNextPhase = nullptr;

public:
    CompositeRun(AutoPtr<IRunPhase>&& first)
        : first(std::move(first)) {}

    void setPhaseCallback(Function<void(const Storage& storage)> callback) {
        onNextPhase = callback;
    }

    virtual void setUp() override {
        first->setUp();
        this->storage = first->getStorage();
    }

    virtual void run() override {
        AutoPtr<IRunPhase> currentHolder;
        RawPtr<IRunPhase> current = first.get(); // references either first or currentHolders
        // we need to hold callbacks, otherwise they would get deleted
        SharedPtr<IRunCallbacks> callbacks = current->callbacks;
        while (true) {
            current->run();
            AutoPtr<IRunPhase> next = current->getNextPhase();
            if (!next) {
                return;
            }
            // make the handoff, using the storage of the previous run
            next->handoff(std::move(*current->getStorage()));
            // copy the callbacks
            /// \todo is this always necessary
            next->callbacks = callbacks;
            this->storage = next->getStorage();
            currentHolder = std::move(next);
            current = currentHolder.get();
            if (onNextPhase) {
                onNextPhase(*storage);
            }
        }
    }

protected:
    virtual void tearDown(const Statistics& UNUSED(stats)) override {}
};

NAMESPACE_SPH_END
