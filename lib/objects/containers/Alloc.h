#pragma once

/// \file Alloc.h
/// \brief Utilities for memory allocation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "common/Assert.h"
#ifndef SPH_MSVC
#include <mm_malloc.h>
#endif

NAMESPACE_SPH_BEGIN

INLINE void* alignedMalloc(const std::size_t size, const std::size_t alignment) {
#ifdef SPH_MSVC
    return _aligned_malloc(size, alignment);
#else
    return _mm_malloc(size, alignment);
#endif
}


NAMESPACE_SPH_END
