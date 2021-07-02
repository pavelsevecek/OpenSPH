#pragma once

/// \file Outcome.h
/// \brief Return value of function that may fail, containing either SUCCEES (true) or error message
/// \author Pavel Sevecek (sevecek ar sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

struct SuccessTag {};

struct FailTag {};

/// \brief Utility functions used within \ref BasicOutcome.
///
/// Has to be specialized for each error type.
template <typename TError>
struct OutcomeTraits {

    /// \brief Helper function returning default error message.
    INLINE static TError defaultError();

    /// \brief Concatenated two error messages.
    INLINE static TError concatenate(const TError& e1, const TError& e2);
};

template <>
struct OutcomeTraits<std::string> {
    INLINE static std::string defaultError() {
        return "ERROR";
    }

    INLINE static std::string concatenate(const std::string& e1, const std::string& e2) {
        return e1 + " AND " + e2;
    }
};

/// \brief Expected-like class that does not contain any value.
///
/// Either contains "success" (no error), or error message. The error message must be default-constructible.
template <typename TError>
class BasicOutcome {
private:
    Optional<TError> e;

public:
    /// \brief Constructs object with success (no error)
    INLINE BasicOutcome(SuccessTag) {}

    /// \brief Constructs object with defautl error message.
    INLINE BasicOutcome(FailTag)
        : e(OutcomeTraits<TError>::defaultError()) {}

    /// \brief Constructs object from boolean result.
    ///
    /// If true, reports success, otherwise reports default error message.
    INLINE explicit BasicOutcome(const bool value) {
        if (!value) {
            e.emplace(OutcomeTraits<TError>::defaultError());
        }
    }

    /// \brief Constructs object given error message.
    template <typename T, typename = std::enable_if_t<std::is_constructible<TError, T>::value>>
    INLINE BasicOutcome(T&& error)
        : e(std::forward<T>(error)) {}

    /// \brief Checks whether the object contains success, i.e. no error is stored.
    INLINE bool success() const {
        return !e;
    }

    /// \brief Conversion to bool, returning true if no error is stored.
    INLINE explicit operator bool() const {
        return success();
    }

    /// \brief Inversion operator
    INLINE bool operator!() const {
        return !success();
    }

    /// \brief Returns the error message.
    ///
    /// If the object contains success (no error), asserts.
    INLINE const TError& error() const {
        SPH_ASSERT(!success());
        return e.value();
    }

    /// \brief Compares two outcomes.
    ///
    /// Outcomes are only considered equal if both are successful or both contain equal error message.
    bool operator==(const BasicOutcome& other) const {
        return (!e && !other.e) || (e == other.e);
    }

    /// \brief Prints "success" or error message into the output stream.
    friend std::ostream& operator<<(std::ostream& stream, const BasicOutcome& outcome) {
        if (outcome) {
            stream << "success";
        } else {
            stream << outcome.error();
        }
        return stream;
    }

    /// \brief Logical 'or' operator, returning SUCCESS if either of the values is a SUCCESS.
    ///
    /// If both values contain an error message, they are concatenated.
    friend BasicOutcome operator||(const BasicOutcome& o1, const BasicOutcome& o2) {
        if (!o1 && !o2) {
            return OutcomeTraits<TError>::concatenate(o1.error(), o2.error());
        }
        return SuccessTag{};
    }

    /// \brief Logical 'and' operator, returning SUCCESS if both values are SUCCESS.
    ///
    /// Returns the error message if one of the values contains an error message. /// If both values contain
    /// an error message, they are concatenated.
    friend BasicOutcome operator&&(const BasicOutcome& o1, const BasicOutcome& o2) {
        if (!o1 && !o2) {
            return OutcomeTraits<TError>::concatenate(o1.error(), o2.error());
        } else if (!o1) {
            return o1.error();
        } else if (!o2) {
            return o2.error();
        } else {
            return SuccessTag{};
        }
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
} // namespace Detail

/// \brief Constructs failed object with error message.
///
/// Error message is constructed by converting arguments to string and concatenating them.
template <typename... TArgs>
INLINE Outcome makeFailed(TArgs&&... args) {
    std::stringstream ss;
    Detail::printArgs(ss, std::forward<TArgs>(args)...);
    SPH_ASSERT(!ss.str().empty());
    return Outcome(ss.str());
}

/// \brief Constructs outcome object given the condition.
///
/// If condition equals false, print error message using argument list.
template <typename... TArgs>
INLINE Outcome makeOutcome(const bool condition, TArgs&&... args) {
    if (condition) {
        return SUCCESS;
    } else {
        std::stringstream ss;
        Detail::printArgs(ss, std::forward<TArgs>(args)...);
        SPH_ASSERT(!ss.str().empty());
        return Outcome(ss.str());
    }
}

NAMESPACE_SPH_END
