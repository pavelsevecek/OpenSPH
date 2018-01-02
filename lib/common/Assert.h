#pragma once

/// \file Assert.h
/// \brief Custom assertions
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "common/Globals.h"
#include <exception>
#include <sstream>

NAMESPACE_SPH_BEGIN

struct Assert {
    static bool isTest;
    static bool breakOnFail;

    class Exception : public std::exception {
    private:
        const std::string message;

    public:
        Exception(const char* message)
            : message(message) {}

        virtual const char* what() const noexcept override {
            return message.c_str();
        }
    };

    struct ScopedBreakDisabler {
        const bool originalValue;

        ScopedBreakDisabler()
            : originalValue(breakOnFail) {
            breakOnFail = false;
        }
        ~ScopedBreakDisabler() {
            breakOnFail = originalValue;
        }
    };

    template <typename T0, typename... TArgs>
    static void stringify(std::stringstream& ss, T0&& t0, TArgs&&... rest) {
        ss << t0;
        if (sizeof...(TArgs) > 0) {
            ss << " ,";
        }
        stringify(ss, std::forward<TArgs>(rest)...);
    }

    static void stringify(std::stringstream& UNUSED(ss)) {}

    template <typename... TArgs>
    static void fire(const char* message,
        const char* file,
        const char* func,
        const int line,
        TArgs&&... args) {
        std::stringstream ss;
        stringify(ss, std::forward<TArgs>(args)...);
        fireParams(message, file, func, line, ss.str().c_str());
    }

    static void fireParams(const char* message,
        const char* file,
        const char* func,
        const int line,
        const char* params = "");

    static void todo(const char* message, const char* func, const int line);
};

#define TODO(x) Assert::todo(x, __FUNCTION__, __LINE__);

#ifdef SPH_DEBUG
#define ASSERT(x, ...)                                                                                       \
    if (!bool(x)) {                                                                                          \
        Assert::fire(#x, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);                                   \
    }
#define CONSTEXPR_ASSERT(x) assert(x)
#else
#define ASSERT(x, ...)
#define CONSTEXPR_ASSERT(x)
#endif

/// Helper macro marking missing implementation
#define NOT_IMPLEMENTED                                                                                      \
    ASSERT(false, "not implemented");                                                                        \
    throw std::exception();

/// Helper macro marking code that should never be executed (default branch of switch where there is finite
/// number of options, for example)
#define STOP                                                                                                 \
    ASSERT(false, "stop");                                                                                   \
    throw std::exception();

/// Helper cast, performing a static_cast, but checking that the cast is valid using dynamic_cast in assert
/// and debug builds.
template <typename TDerived, typename TBase>
INLINE TDerived assert_cast(TBase* value) {
    static_assert(std::is_pointer<TDerived>::value, "Must be a pointer type");
    ASSERT(!value || dynamic_cast<TDerived>(value) != nullptr, value);
    return static_cast<TDerived>(value);
}


NAMESPACE_SPH_END
