#include "thread/OpenMp.h"
#include "math/MathBasic.h"
#include "objects/wrappers/Optional.h"
#include "thread/CheckFunction.h"

#ifdef SPH_USE_OPENMP
#include <omp.h>
#endif

NAMESPACE_SPH_BEGIN

#ifdef SPH_USE_OPENMP

SharedPtr<OmpScheduler> OmpScheduler::globalInstance;

OmpScheduler::OmpScheduler(const Size numThreads) {
    if (numThreads > 0) {
        omp_set_num_threads(numThreads);
    }
}

class OmpTaskHandle : public ITask {
public:
    virtual void wait() override {}

    virtual bool completed() const override {
        return true;
    }
};


SharedPtr<ITask> OmpScheduler::submit(const Function<void()>& task) {
    if (isMainThread()) {
#pragma omp parallel
        {
#pragma omp single
            {
#pragma omp taskgroup
                {
#pragma omp task
                    { task(); }
                }
            }
        }
    } else {
#pragma omp task
        { task(); }
    }
    return makeShared<OmpTaskHandle>();
}

Optional<Size> OmpScheduler::getThreadIdx() const {
    return omp_get_thread_num();
}

Size OmpScheduler::getThreadCnt() const {
    return omp_get_max_threads();
}

Size OmpScheduler::getRecommendedGranularity() const {
    return granularity;
}

void OmpScheduler::parallelFor(const Size from,
    const Size to,
    const Size granularity,
    const Function<void(Size n1, Size n2)>& functor) {
#pragma omp parallel for schedule(dynamic, 1)
    for (Size n = from; n < to; n += granularity) {
        const Size n1 = n;
        const Size n2 = min(n1 + granularity, to);
        functor(n1, n2);
    }
}

SharedPtr<OmpScheduler> OmpScheduler::getGlobalInstance() {
    if (!globalInstance) {
        globalInstance = makeShared<OmpScheduler>();
    }
    return globalInstance;
}

#endif

NAMESPACE_SPH_END
