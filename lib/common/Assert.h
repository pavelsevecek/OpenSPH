#pragma once

#include "common/Globals.h"
#include <exception>
#include <sstream>

NAMESPACE_SPH_BEGIN

struct Assert {
    static bool isTest;
    static bool breakOnFail;

    class Exception : public std::exception {
    private:
        const char* message;

    public:
        Exception(const char* message)
            : message(message) {}

        virtual const char* what() const noexcept override {
            return message;
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
        stringify(ss, std::forward<TArgs>(rest)...);
    }

    static void stringify(std::stringstream& UNUSED(ss)) {}

    template <typename T0, typename... TArgs>
    static void check(const bool condition,
        const char* message,
        const char* file,
        const char* func,
        const int line,
        T0&& t0,
        TArgs&&... args) {
        std::stringstream ss;
        stringify(ss, std::forward<T0>(t0), std::forward<TArgs>(args)...);
        check(condition, message, file, func, line, ss.str().c_str());
    }

    static void check(const bool condition,
        const char* message,
        const char* file,
        const char* func,
        const int line,
        const char* params = "");

    static void todo(const char* message, const char* func, const int line);
};

#define TODO(x) Assert::todo(x, __FUNCTION__, __LINE__);

#ifdef SPH_DEBUG
#define ASSERT(x, ...) Assert::check(bool(x), #x, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define CONSTEXPR_ASSERT(x) assert(x)
#else
#define ASSERT(x, ...)
#define CONSTEXPR_ASSERT(x)
#endif

/// Helper macro marking missing implementation
#define NOT_IMPLEMENTED                                                                                      \
    ASSERT(false && "not implemented");                                                                      \
    throw std::exception();

/// Helper macro marking code that should never be executed (default branch of switch where there is finite
/// number of options, for example)
#define STOP                                                                                                 \
    ASSERT(false && "stop");                                                                                 \
    throw std::exception();

NAMESPACE_SPH_END
