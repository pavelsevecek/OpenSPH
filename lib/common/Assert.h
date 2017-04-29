#pragma once

#include "common/Globals.h"
#include <exception>

// forward-declare std::string

namespace std {
    template <class Char>
    struct char_traits;

    template <typename T>
    class allocator;

    template <typename Char, typename Traits, typename Allocator>
    class basic_string;

    typedef basic_string<char, char_traits<char>, allocator<char>> string;
}

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

    static void check(const bool condition,
        const char* message,
        const char* file,
        const char* func,
        const int line,
        const std::string& params);

    static void check(const bool condition,
        const char* message,
        const char* file,
        const char* func,
        const int line);

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
