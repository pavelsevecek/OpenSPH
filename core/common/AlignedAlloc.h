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

INLINE constexpr std::size_t roundToAlignment(const std::size_t value, const std::size_t align) noexcept {
    const std::size_t remainder = value % align;
    return value + ((remainder == 0) ? 0 : (align - remainder));
}

INLINE constexpr std::size_t roundUpToPower2(std::size_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    return v;
}

INLINE void* alignedAlloc(std::size_t size, std::size_t align) noexcept {
#ifdef SPH_ARM
    align = roundUpToPower2(align);
    if (align < 16) {
        align = 16;
    }
    size = roundToAlignment(size, align);
    return std::aligned_alloc(align, size);
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
