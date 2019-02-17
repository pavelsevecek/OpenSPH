#pragma once

/// \file Utils.h
/// \brief Additional helper macros and functions for unit testing
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "catch.hpp"
#include "io/Logger.h"
#include "math/rng/Rng.h"
#include "objects/containers/Array.h"
#include "objects/geometry/Vector.h"
#include <mutex>

#ifdef SPH_DEBUG
#define REQUIRE_ASSERT(func)                                                                                 \
    {                                                                                                        \
        Sph::Assert::ScopedAssertExceptionEnabler enabler;                                                   \
        REQUIRE_THROWS((void)(func));                                                                        \
    }
#else
#define REQUIRE_ASSERT(func)
#endif

extern std::mutex globalTestMutex;

#define REQUIRE_THREAD_SAFE(func)                                                                            \
    {                                                                                                        \
        std::unique_lock<std::mutex> lock(globalTestMutex);                                                  \
        REQUIRE(func);                                                                                       \
    }

extern Sph::Array<std::pair<std::string, int>> skippedTests;

#define SKIP_TEST                                                                                            \
    {                                                                                                        \
        StdOutLogger logger;                                                                                 \
        logger.write(" << Test in file ", __FILE__, " on line ", __LINE__, " temporarily disabled");         \
        skippedTests.push(std::make_pair(__FILE__, __LINE__));                                               \
        return;                                                                                              \
    }


NAMESPACE_SPH_BEGIN

/// Returns a random vector. Components of integral types range from -5 to 5, for floating point types the
/// range is -0.5 to 0.5.
INLINE Vector randomVector() {
    const Float range = 1._f;
    static UniformRng rng;
    Vector v;
    for (int i = 0; i < 3; ++i) {
        v[i] = Float(range * (rng() - 0.5_f));
    }
    return v;
}

NAMESPACE_SPH_END
