#include "thread/Tbb.h"
#include "math/MathUtils.h"
#include "objects/wrappers/Optional.h"

#ifdef SPH_USE_TBB
#include <tbb/scalable_allocator.h>
#include <tbb/tbb.h>
#endif

NAMESPACE_SPH_BEGIN

#ifdef SPH_USE_TBB

SharedPtr<Tbb> Tbb::globalInstance = nullptr;

inline Size maxThreadCnt() {
#if TBB_VERSION_MAJOR >= 2021
    return Size(tbb::global_control::active_value(tbb::global_control::max_allowed_parallelism));
#else
    return Size(tbb::task_scheduler_init::default_num_threads());
#endif
}

struct TbbData {
    tbb::task_arena arena;
    Size granularity;

    TbbData(const Size numThreads, const Size granularity)
        : granularity(granularity) {
        arena.initialize(numThreads > 0 ? numThreads : maxThreadCnt());
    }
};

Tbb::Tbb(const Size numThreads, const Size granularity) {
    data = makeAuto<TbbData>(numThreads, granularity);
}

Tbb::~Tbb() = default;

void Tbb::setGranularity(const Size newGranularity) {
    data->granularity = newGranularity;
}

void Tbb::setThreadCnt(const Size numThreads) {
    data = makeAuto<TbbData>(numThreads, data->granularity);
}

Optional<Size> Tbb::getThreadIdx() const {
    const int index = tbb::this_task_arena::current_thread_index();
    if (index >= 0) {
        return index;
    } else {
        return NOTHING;
    }
}

Size Tbb::getThreadCnt() const {
    return tbb::this_task_arena::max_concurrency();
}

Size Tbb::getRecommendedGranularity() const {
    return data->granularity;
}

void Tbb::parallelFor(const Size from, const Size to, const Size granularity, const RangeFunctor& functor) {
    data->arena.execute([from, to, granularity, &functor] {
        tbb::parallel_for(tbb::blocked_range<Size>(from, to, granularity),
            [&functor](const tbb::blocked_range<Size> range) { functor(range.begin(), range.end()); });
    });
}

void Tbb::parallelInvoke(const Functor& task1, const Functor& task2) {
    data->arena.execute([&task1, &task2] { tbb::parallel_invoke(task1, task2); });
}

SharedPtr<Tbb> Tbb::getGlobalInstance() {
    if (!globalInstance) {
        globalInstance = makeShared<Tbb>();
    }
    return globalInstance;
}

MemoryBlock TbbAllocator::allocate(const std::size_t size, const std::size_t align) noexcept {
    MemoryBlock block;
    block.ptr = scalable_aligned_malloc(size, align);
    if (block.ptr) {
        block.size = size;
    } else {
        block.size = 0;
    }
    return block;
}

void TbbAllocator::deallocate(MemoryBlock& block) noexcept {
    scalable_aligned_free(block.ptr);
    block.ptr = nullptr;
}

#endif

NAMESPACE_SPH_END
