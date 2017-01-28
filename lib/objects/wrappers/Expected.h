#pragma once

/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/wrappers/Variant.h"

NAMESPACE_SPH_BEGIN

/// Wrapper of type that either contains a value of given type, or an error message. If the type of error
/// message is bool, this is essentially identical to Optional.
/// Expected is designed as a return type.
///
/// When talking about 'expected' value, it means no error has been encounter and Expected contains value of
/// given type;
/// 'unexpected' value means that Expected contains an error message.
///
/// Inspired by Andrei Alexandrescu - Systematic Error Handling in C++
/// https://channel9.msdn.com/Shows/Going+Deep/C-and-Beyond-2012-Andrei-Alexandrescu-Systematic-Error-Handling-in-C

struct UnexpectedTag {};
const UnexpectedTag UNEXPECTED;

template <typename Type, typename Error = std::string>
class Expected {
private:
    Variant<Type, Error> data;

public:
    /// Constructs an expected value.
    template <typename T, typename = std::enable_if_t<std::is_constructible<Type, T>::value>>
    Expected(T&& value) {
        data.emplace<Type>(std::forward<T>(value));
    }

    /// Constructs an unexpected value.
    template <typename TError, typename = std::enable_if_t<std::is_constructible<Error, TError>::value>>
    Expected(UnexpectedTag, TError&& error) {
        data.emplace<Error>(std::forward<TError>(error));
    }

    /// Conversion to bool, checking whether object constains expected value.
    operator bool() const { return isExpected(); }

    /// Negation operator, returns true if object does NOT contain expected value.
    bool operator!() const { return !isExpected(); }

    /// Returns the reference to expected value. Object must not contain unexpected value, checked by assert.
    Type& get() {
        ASSERT(isExpected());
        return data.get<Type>();
    }

    /// Returns the const reference to expected value. Object must not contain unexpected value, checked by
    /// assert.
    const Type& get() const {
        ASSERT(isExpected());
        return data.get<Type>();
    }

private:
    bool isExpected() const { return data.getTypeIdx() == 0; }
};

/// Constructs an unexpected value of given type, given error message as std::string. For other type of error
/// messages, use constructor of Expected.
template <typename Type>
Expected<Type> makeUnexpected(const std::string& error) {
    return Expected<Type>(UNEXPECTED, error);
}

NAMESPACE_SPH_END
