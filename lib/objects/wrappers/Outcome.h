#pragma once

/// Pavel Sevecek 2017
/// sevecek ar sirrah.troja.mff.cuni.cz

#include "common/Assert.h"
#include <sstream>

NAMESPACE_SPH_BEGIN

struct SuccessTag {};

struct FailTag {};

/// Expected-like class that does not contain any value. Either contains "success" (no error), or error
/// message. Unlike Expected, error message is assumed to be std::string, for simplicity.
class Outcome {
private:
    std::string e;

    static constexpr auto defaultError = "error";

public:
    /// Constructs object with success (no error)
    INLINE Outcome(SuccessTag) {}

    /// Constructs object with defautl error message.
    INLINE Outcome(FailTag)
        : e(defaultError) {}

    /// Constructs object from boolean result; if true, reports success, otherwise reports default error
    /// message.
    INLINE Outcome(const bool value)
        : e(value ? "" : defaultError) {}

    /// Constructs object given error message.
    INLINE Outcome(const std::string& error)
        : e(error) {
        ASSERT(!e.empty());
    }

    /// Checks whether the object contains success, i.e. no error is stored.
    INLINE bool success() const {
        return e.empty();
    }

    /// Conversion to bool, returning true if no error is stored.
    INLINE operator bool() const {
        return success();
    }

    /// Inversion operator
    INLINE bool operator!() const {
        return !success();
    }

    /// Returns the error message. If the object contains success (no error), asserts.
    INLINE const std::string& error() const {
        ASSERT(!success());
        return e;
    }
};

/// Global constant for successful outcome
const Outcome SUCCESS{ SuccessTag{} };

namespace Detail {
    INLINE void printArgs(std::stringstream&) {}

    template <typename T0, typename... TArgs>
    INLINE void printArgs(std::stringstream& ss, T0&& t0, TArgs&&... args) {
        ss << t0;
        printArgs(ss, std::forward<TArgs>(args)...);
    }
}

/// Constructs failed object with error message. Error message is consturcted by converting arguments to
/// string and concating them.
template <typename... TArgs>
INLINE Outcome makeFailed(TArgs&&... args) {
    std::stringstream ss;
    Detail::printArgs(ss, std::forward<TArgs>(args)...);
    ASSERT(!ss.str().empty());
    return Outcome(ss.str());
}

/// Constructs outcome object given the condition. If condition equals false, print error message using
/// argument list.
template <typename... TArgs>
INLINE Outcome makeOutcome(const bool condition, TArgs&&... args) {
    if (condition) {
        return SUCCESS;
    } else {
        std::stringstream ss;
        Detail::printArgs(ss, std::forward<TArgs>(args)...);
        ASSERT(!ss.str().empty());
        return Outcome(ss.str());
    }
}

NAMESPACE_SPH_END
