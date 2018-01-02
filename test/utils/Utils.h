#pragma once

/// \file Utils.h
/// \brief Additional helper macros and functions for unit testing
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "catch.hpp"
#include "io/Logger.h"
#include "math/rng/Rng.h"
#include "objects/geometry/Vector.h"

/// Test cases for testing of multiple types

#define CATCH_TESTCASE1(...)                                                                                 \
    static void INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_S_T____1)();                                 \
    namespace {                                                                                              \
        Catch::AutoReg INTERNAL_CATCH_UNIQUE_NAME(autoRegistrar1)(                                           \
            &INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_S_T____1),                                      \
            CATCH_INTERNAL_LINEINFO,                                                                         \
            Catch::NameAndDesc(__VA_ARGS__));                                                                \
    }                                                                                                        \
    static void INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_S_T____1)()

#define CATCH_TESTCASE2(...)                                                                                 \
    static void INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_S_T____2)();                                 \
    namespace {                                                                                              \
        Catch::AutoReg INTERNAL_CATCH_UNIQUE_NAME(autoRegistrar2)(                                           \
            &INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_S_T____2),                                      \
            CATCH_INTERNAL_LINEINFO,                                                                         \
            Catch::NameAndDesc(__VA_ARGS__));                                                                \
    }                                                                                                        \
    static void INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_S_T____2)()

#define CATCH_STRINGIFY(x, y) x "  [" y "]"

#define TYPED_TEST_CASE_2(name, description, T, T1, T2)                                                      \
    template <typename T>                                                                                    \
    void INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_M_P_L_A_TE____T_E_S_T____)();                       \
    CATCH_TESTCASE1(CATCH_STRINGIFY(name, #T1), description) {                                               \
        INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_M_P_L_A_TE____T_E_S_T____)<T1>();                    \
    }                                                                                                        \
    CATCH_TESTCASE2(CATCH_STRINGIFY(name, #T2), description) {                                               \
        INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_M_P_L_A_TE____T_E_S_T____)<T2>();                    \
    }                                                                                                        \
    template <typename T>                                                                                    \
    void INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_M_P_L_A_TE____T_E_S_T____)()


#ifdef SPH_DEBUG
#define REQUIRE_ASSERT(func)                                                                                 \
    {                                                                                                        \
        Sph::Assert::ScopedBreakDisabler disabler;                                                           \
        REQUIRE_THROWS((void)(func));                                                                        \
    }
#else
#define REQUIRE_ASSERT(func)
#endif

#define SKIP_TEST                                                                                            \
    {                                                                                                        \
        StdOutLogger logger;                                                                                 \
        logger.write(" << Test in file ", __FILE__, " on line ", __LINE__, " temporarily disabled");         \
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
