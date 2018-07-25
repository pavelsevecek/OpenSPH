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

#define CATCH_TESTCASE3(...)                                                                                 \
    static void INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_S_T____3)();                                 \
    namespace {                                                                                              \
        Catch::AutoReg INTERNAL_CATCH_UNIQUE_NAME(autoRegistrar3)(                                           \
            &INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_S_T____3),                                      \
            CATCH_INTERNAL_LINEINFO,                                                                         \
            Catch::NameAndDesc(__VA_ARGS__));                                                                \
    }                                                                                                        \
    static void INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_S_T____3)()

#define CATCH_TESTCASE4(...)                                                                                 \
    static void INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_S_T____4)();                                 \
    namespace {                                                                                              \
        Catch::AutoReg INTERNAL_CATCH_UNIQUE_NAME(autoRegistrar4)(                                           \
            &INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_S_T____4),                                      \
            CATCH_INTERNAL_LINEINFO,                                                                         \
            Catch::NameAndDesc(__VA_ARGS__));                                                                \
    }                                                                                                        \
    static void INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_S_T____4)()

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

#define TYPED_TEST_CASE_3(name, description, T, T1, T2, T3)                                                  \
    template <typename T>                                                                                    \
    void INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_M_P_L_A_TE____T_E_S_T____)();                       \
    CATCH_TESTCASE1(CATCH_STRINGIFY(name, #T1), description) {                                               \
        INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_M_P_L_A_TE____T_E_S_T____)<T1>();                    \
    }                                                                                                        \
    CATCH_TESTCASE2(CATCH_STRINGIFY(name, #T2), description) {                                               \
        INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_M_P_L_A_TE____T_E_S_T____)<T2>();                    \
    }                                                                                                        \
    CATCH_TESTCASE3(CATCH_STRINGIFY(name, #T3), description) {                                               \
        INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_M_P_L_A_TE____T_E_S_T____)<T3>();                    \
    }                                                                                                        \
    template <typename T>                                                                                    \
    void INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_M_P_L_A_TE____T_E_S_T____)()


#define TYPED_TEST_CASE_4(name, description, T, T1, T2, T3, T4)                                              \
    template <typename T>                                                                                    \
    void INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_M_P_L_A_TE____T_E_S_T____)();                       \
    CATCH_TESTCASE1(CATCH_STRINGIFY(name, #T1), description) {                                               \
        INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_M_P_L_A_TE____T_E_S_T____)<T1>();                    \
    }                                                                                                        \
    CATCH_TESTCASE2(CATCH_STRINGIFY(name, #T2), description) {                                               \
        INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_M_P_L_A_TE____T_E_S_T____)<T2>();                    \
    }                                                                                                        \
    CATCH_TESTCASE3(CATCH_STRINGIFY(name, #T3), description) {                                               \
        INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_M_P_L_A_TE____T_E_S_T____)<T3>();                    \
    }                                                                                                        \
    CATCH_TESTCASE4(CATCH_STRINGIFY(name, #T4), description) {                                               \
        INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_M_P_L_A_TE____T_E_S_T____)<T4>();                    \
    }                                                                                                        \
    template <typename T>                                                                                    \
    void INTERNAL_CATCH_UNIQUE_NAME(____C_A_T_C_H____T_E_M_P_L_A_TE____T_E_S_T____)()

// Add more when needed


#ifdef SPH_DEBUG
#define REQUIRE_ASSERT(func)                                                                                 \
    {                                                                                                        \
        Sph::Assert::ScopedBreakDisabler disabler;                                                           \
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
