#pragma once

/// \file CheckFunction.h
/// \brief Helper functions to check the internal consistency of the code
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/Globals.h"
#include "objects/wrappers/Flags.h"
#include <atomic>

NAMESPACE_SPH_BEGIN

enum class CheckFunction {
    NON_REENRANT = 1 << 0,    ///< Function can be executed by any thread, but only once at a time
    MAIN_THREAD = 1 << 1,     ///< Function can only be executed from main thread
    NOT_MAIN_THREAD = 1 << 2, ///< Function cannot be called from main thread
    ONCE = 1 << 3,            ///< Function can be executed only once in the application
    NO_THROW = 1 << 4,        ///< Function cannot throw exceptions
};

class FunctionChecker {
    std::atomic<Size>& reentrantCnt;
    Flags<CheckFunction> flags;

public:
    FunctionChecker(std::atomic<Size>& reentrantCnt,
        std::atomic<Size>& totalCnt,
        const Flags<CheckFunction> flags);

    ~FunctionChecker();
};

#ifdef SPH_DEBUG
#define CHECK_FUNCTION(flags)                                                                                \
    static std::atomic<Size> __reentrantCnt;                                                                 \
    static std::atomic<Size> __totalCnt;                                                                     \
    FunctionChecker __checker(__reentrantCnt, __totalCnt, flags);
#else
#define CHECK_FUNCTION(flags)
#endif


/// Checks if the calling thread is the main thred
bool isMainThread();

NAMESPACE_SPH_END
