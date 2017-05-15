#pragma once

/// \file Outcome.h
/// \brief Return value of function that may fail, containing either SUCCEES (true) or error message
/// \author Pavel Sevecek (sevecek ar sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

struct SuccessTag {};

struct FailTag {};

namespace OutcomeTraits {
    /// Helper function returning default error of outcome. Error type must either be default constructible,
    /// or the function must be specialized for given type.
    template <typename TError>
    INLINE TError defaultError() {
        return TError();
    }

    template <>
    INLINE std::string defaultError<std::string>() {
        return "error";
    }
}

/// Expected-like class that does not contain any value. Either contains "success" (no error), or error
/// message. The error message must be default-constructible.
template <typename TError>
class BasicOutcome {
private:
    Optional<TError> e;

public:
    /// Constructs object with success (no error)
    INLINE BasicOutcome(SuccessTag) {}

    /// Constructs object with defautl error message.
    INLINE BasicOutcome(FailTag)
        : e(OutcomeTraits::defaultError<TError>()) {}

    /// Constructs object from boolean result; if true, reports success, otherwise reports default error
    /// message.
    INLINE explicit BasicOutcome(const bool value) {
        if (!value) {
            e.emplace(OutcomeTraits::defaultError<TError>());
        }
    }

    /// Constructs object given error message.
    template <typename T, typename = std::enable_if_t<std::is_constructible<TError, T>::value>>
    INLINE BasicOutcome(T&& error)
        : e(std::forward<T>(error)) {}

    /// Checks whether the object contains success, i.e. no error is stored.
    INLINE bool success() const {
        return !e;
    }

    /// Conversion to bool, returning true if no error is stored.
    INLINE explicit operator bool() const {
        return success();
    }

    /// Inversion operator
    INLINE bool operator!() const {
        return !success();
    }

    /// Returns the error message. If the object contains success (no error), asserts.
    INLINE const TError& error() const {
        ASSERT(!success());
        return e.value();
    }

    /// Compares two outcomes. Outcomes are only considered equal if both are successful or both contain equal
    /// error message.
    bool operator==(const BasicOutcome& other) const {
        return e == other.e;
    }

    /// Prints "success" or error message into the output stream.
    friend std::ostream& operator<<(std::ostream& stream, const BasicOutcome& outcome) {
        if (outcome) {
            stream << "success";
        } else {
            stream << outcome.error();
        }
        return stream;
    }
};

/// Alias for string error message
using Outcome = BasicOutcome<std::string>;

/// Global constant for successful outcome
const SuccessTag SUCCESS;

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
