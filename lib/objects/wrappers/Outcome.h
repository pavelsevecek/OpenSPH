#pragma once

/// \file Outcome.h
/// \brief Return value of function that may fail, containing either SUCCEES (true) or error message
/// \author Pavel Sevecek (sevecek ar sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/Assert.h"
#include <sstream>

NAMESPACE_SPH_BEGIN

struct SuccessTag {};

struct FailTag {};

/// Expected-like class that does not contain any value. Either contains "success" (no error), or error
/// message. The error message must be default-constructible.
template<typename TError>
class BasicOutcome {
private:
    Optional<TError> e;

    const static TError defaultError = TError();

public:
    /// Constructs object with success (no error)
    INLINE Outcome(SuccessTag) {}

    /// Constructs object with defautl error message.
    INLINE Outcome(FailTag)
        : e(defaultError) {}

    /// Constructs object from boolean result; if true, reports success, otherwise reports default error
    /// message.
    INLINE Outcome(const bool value) {
		if (value) {
			e.emplace(defaultError);
		}
	}

    /// Constructs object given error message using copy constructor.
    INLINE Outcome(const TError& error)
        : e(error) {}
		
	/// Constructs object given error message using move constructor.
	INLINE Outcome(TError&& error)
        : e(std::move(error)) {}

    /// Constructs object given error message. Overload for char pointer, it is needed otherwise constructing
    /// Outcome from char* would call the bool constructor.
    INLINE Outcome(const char* error)
        : e(error) {
        ASSERT(!e.empty());
    }

    /// Checks whether the object contains success, i.e. no error is stored.
    INLINE bool success() const {
        return bool(e);
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
        return e;
    }

    /// Compares two outcomes. Outcomes are only considered equal if both are successful or both contain equal
    /// error message.
    bool operator==(const Outcome& other) const {
        return e == other.e;
    }

    /// Prints "success" or error message into the output stream.
    friend std::ostream& operator<<(std::ostream& stream, const Outcome& outcome) {
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
