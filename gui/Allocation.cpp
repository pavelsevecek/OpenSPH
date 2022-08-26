#include "common/AlignedAlloc.h"

/// Ensure correct alignment of Vectors

void* operator new(std::size_t s) {
#ifdef SPH_SINGLE_PRECISION
    return Sph::alignedAlloc(s, 16);
#else
    return Sph::alignedAlloc(s, 32);
#endif
}

void operator delete(void* p) {
    Sph::alignedFree(p);
}

void operator delete(void* p, std::size_t UNUSED(sz)) {
    Sph::alignedFree(p);
}
