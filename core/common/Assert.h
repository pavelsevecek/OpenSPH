#pragma once

/// \file Assert.h
/// \brief Custom assertions
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/Globals.h"
#include <exception>
#include <sstream>

NAMESPACE_SPH_BEGIN

struct Assert {
    static bool isTest;

    /// \brief If true, assert throws an exception.
    static bool throwAssertException;

    typedef bool (*Handler)(const std::string& message);

    /// \brief Custom assert handler.
    ///
    /// Assert message is passed as an argument. If it returns false, the assert is ignored and the program
    /// continues. Note that assert can be fired from any thread, so it has to be thread-safe.
    static Handler handler;

    class Exception : public std::exception {
    private:
        const std::string message;

    public:
        Exception(const std::string& message)
            : message(message) {}

        virtual const char* what() const noexcept override {
            return message.c_str();
        }
    };

    struct ScopedAssertExceptionEnabler {
        const bool originalValue;

        ScopedAssertExceptionEnabler()
            : originalValue(throwAssertException) {
            throwAssertException = true;
        }
        ~ScopedAssertExceptionEnabler() {
            throwAssertException = originalValue;
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
#define SPH_ASSERT(x, ...)                                                                                   \
    if (!bool(x)) {                                                                                          \
        Assert::fire(#x, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);                                   \
    }
#define CONSTEXPR_SPH_ASSERT(x) assert(x)
#define SPH_ASSERT_UNEVAL(x, ...) SPH_ASSERT(x, ##__VA_ARGS__)
#else
#define SPH_ASSERT(x, ...) MARK_USED(x)
#define CONSTEXPR_SPH_ASSERT(x) MARK_USED(x)
#define SPH_ASSERT_UNEVAL(x, ...)
#endif

/// Helper macro marking missing implementation
#define NOT_IMPLEMENTED                                                                                      \
    SPH_ASSERT(false, "not implemented");                                                                    \
    throw Assert::Exception(std::string("Functionality not implemented in function ") + SPH_PRETTY_FUNCTION);

/// Helper macro marking code that should never be executed (default branch of switch where there is finite
/// number of options, for example)
#define STOP                                                                                                 \
    SPH_ASSERT(false, "stop");                                                                               \
    throw std::exception();

/// Helper cast, performing a static_cast, but checking that the cast is valid using dynamic_cast in assert
/// and debug builds.
template <typename TDerived, typename TBase>
INLINE TDerived* assertCast(TBase* value) {
    SPH_ASSERT(!value || dynamic_cast<TDerived*>(value) != nullptr, value);
    return static_cast<TDerived*>(value);
}

#define SPH_STR(x) SPH_XSTR(x)
#define SPH_XSTR(x) #x

NAMESPACE_SPH_END
