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
    const RangeFunc& functor) {
#pragma omp parallel for schedule(dynamic, 1)
    for (Size n = from; n < to; n += granularity) {
        const Size n1 = n;
        const Size n2 = min(n1 + granularity, to);
        functor(n1, n2);
    }
}

static void invoke(const OmpScheduler::PlainFunc& task1, const OmpScheduler::PlainFunc& task2) {
#pragma omp taskgroup
    {
#pragma omp task shared(task1)
        task1();

#pragma omp task shared(task2)
        task2();
    }
}

void OmpScheduler::parallelInvoke(const PlainFunc& task1, const PlainFunc& task2) {
    if (omp_get_level() == 0) {
// top-level call
#pragma omp parallel shared(task1, task2)
        {
#pragma omp single
            { invoke(task1, task2); }
        }
    } else {
        // nested call
        invoke(task1, task2);
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
