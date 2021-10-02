#include "objects/wrappers/AlignedStorage.h"

/// Ensure correct alignment of Vectors

void* operator new(std::size_t s) {
#ifdef SPH_SINGLE_PRECISION
    return _mm_malloc(s, 16);
#else
    return _mm_malloc(s, 32);
#endif
}

void operator delete(void* p) {
    _mm_free(p);
}

void operator delete(void* p, std::size_t UNUSED(sz)) {
    _mm_free(p);
}
