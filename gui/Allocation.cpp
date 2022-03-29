#include "objects/wrappers/AlignedStorage.h"

/// Ensure correct alignment of Vectors

void* operator new(std::size_t s) {
#ifdef SPH_SINGLE_PRECISION
    return alignedAlloc(s, 16);
#else
    return alignedAlloc(s, 32);
#endif
}

void operator delete(void* p) {
    alignedFree(p);
}

void operator delete(void* p, std::size_t UNUSED(sz)) {
    alignedFree(p);
}
