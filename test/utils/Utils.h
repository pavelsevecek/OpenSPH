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

#define INTERNAL_CATCH_TEMPLATE_TEST_CASE_DECL(name, description, T)                                         \
    template <typename T>                                                                                    \
    void INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_M_P_L_A_TE____T_E_S_T____)();                       \
    TEST_CASE(name, description)

#define INTERNAL_CATCH_TEMPLATE_TEST_CASE_SECTION(Tn)                                                        \
    SECTION(#Tn) {                                                                                           \
        INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_M_P_L_A_TE____T_E_S_T____)<Tn>();                    \
    }

#define INTERNAL_CATCH_TEMPLATE_TEST_CASE_DEFN(T)                                                            \
    template <typename T>                                                                                    \
    void INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_M_P_L_A_TE____T_E_S_T____)()

#define CATCH_TEMPLATE_TEST_CASE_1(name, description, T, T1)                                                 \
    INTERNAL_CATCH_TEMPLATE_TEST_CASE_DECL(name, description, T){ INTERNAL_CATCH_TEMPLATE_TEST_CASE_SECTION( \
        T1) } INTERNAL_CATCH_TEMPLATE_TEST_CASE_DEFN(T)

#define CATCH_TEMPLATE_TEST_CASE_2(name, description, T, T1, T2)                                             \
    INTERNAL_CATCH_TEMPLATE_TEST_CASE_DECL(name, description, T){ INTERNAL_CATCH_TEMPLATE_TEST_CASE_SECTION( \
        T1) INTERNAL_CATCH_TEMPLATE_TEST_CASE_SECTION(T2) } INTERNAL_CATCH_TEMPLATE_TEST_CASE_DEFN(T)

#define CATCH_TEMPLATE_TEST_CASE_3(name, description, T, T1, T2, T3)                                         \
    INTERNAL_CATCH_TEMPLATE_TEST_CASE_DECL(name, description, T){ INTERNAL_CATCH_TEMPLATE_TEST_CASE_SECTION( \
        T1) INTERNAL_CATCH_TEMPLATE_TEST_CASE_SECTION(T2)                                                    \
            INTERNAL_CATCH_TEMPLATE_TEST_CASE_SECTION(T3) } INTERNAL_CATCH_TEMPLATE_TEST_CASE_DEFN(T)

#define CATCH_TEMPLATE_TEST_CASE_4(name, description, T, T1, T2, T3, T4)                                     \
    INTERNAL_CATCH_TEMPLATE_TEST_CASE_DECL(name, description, T){ INTERNAL_CATCH_TEMPLATE_TEST_CASE_SECTION( \
        T1) INTERNAL_CATCH_TEMPLATE_TEST_CASE_SECTION(T2) INTERNAL_CATCH_TEMPLATE_TEST_CASE_SECTION(T3)      \
            INTERNAL_CATCH_TEMPLATE_TEST_CASE_SECTION(T4) } INTERNAL_CATCH_TEMPLATE_TEST_CASE_DEFN(T)


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
