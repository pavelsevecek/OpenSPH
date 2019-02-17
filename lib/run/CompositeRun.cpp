#include "run/CompositeRun.h"
#include "io/Logger.h"
#include "run/RunCallbacks.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

void IRunPhase::doDryRun() {
    settings.set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 0._f));
    logger = makeAuto<NullLogger>();
}

void CompositeRun::setUp() {
    ASSERT(first);
    first->setUp();
    storage = first->getStorage();

    if (callbacks) {
        // override the callbacks by shared value
        first->callbacks = callbacks;
    }
}

void CompositeRun::run() {
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

NAMESPACE_SPH_END
