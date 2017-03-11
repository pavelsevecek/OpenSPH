#pragma once

#include "objects/Object.h"
#include <cassert>
#include <exception>

NAMESPACE_SPH_BEGIN

struct Assert {
    static bool isTest;

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

    static void check(const bool condition, const char* message);
};

#ifdef DEBUG
#define ASSERT(x) Assert::check(bool(x), #x)
#define CONSTEXPR_ASSERT(x) assert(x)
#else
#define ASSERT(x)
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
