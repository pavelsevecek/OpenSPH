#pragma once

#include "objects/Object.h"

#ifndef SPH_WIN
#ifdef SPH_ARM
#include <cstdlib>
#else
#include <mm_malloc.h>
#endif
#else
#include <malloc.h>
#endif

NAMESPACE_SPH_BEGIN

INLINE void* alignedAlloc(std::size_t size, std::size_t align) noexcept {
#ifdef SPH_ARM
    return std::aligned_alloc(size, align);
#else
    return _mm_malloc(size, align);
#endif
}

INLINE void alignedFree(void* ptr) noexcept {
#ifdef SPH_ARM
    std::free(ptr);
#else
    _mm_free(ptr);
#endif
}

NAMESPACE_SPH_END
