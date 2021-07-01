#pragma once

/// \file Expected.h
/// \brief Wrapper of type containing either a value or an error message
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "objects/wrappers/Variant.h"

NAMESPACE_SPH_BEGIN


struct UnexpectedTag {};
const UnexpectedTag UNEXPECTED;

/// \brief Wrapper of type that either contains a value of given type, or an error message.
///
/// Expected is designed as a return type. When talking about 'expected' value, it means no error has been
/// encounter and Expected contains value of given type; 'unexpected' value means that Expected contains an
/// error message.
///
/// Inspired by Andrei Alexandrescu - Systematic Error Handling in C++
/// https://channel9.msdn.com/Shows/Going+Deep/C-and-Beyond-2012-Andrei-Alexandrescu-Systematic-Error-Handling-in-C
template <typename Type, typename Error = std::string>
class Expected {
private:
    /// Wrapper to avoid issues if the value type is the same as the error
    struct UnexpectedWrapper {
        Error error;
    };

    // first type is dummy to use the default constructor of variant
    Variant<NothingType, Type, UnexpectedWrapper> data;

public:
    /// \brief Construct the expected value using default constructor.
    ///
    /// Should be avoided if possible as Expected is mainly designed as a value returned from function, but
    /// nevertheless the default constructor is defined for convenience.
    Expected() {
        data.template emplace<Type>();
    }

    /// Constructs an expected value.
    template <typename T, typename = std::enable_if_t<std::is_constructible<Type, T>::value>>
    Expected(T&& value) {
        data.template emplace<Type>(std::forward<T>(value));
    }

    /// Constructs an unexpected value.
    template <typename TError, typename = std::enable_if_t<std::is_constructible<Error, TError>::value>>
    Expected(UnexpectedTag, TError&& error) {
        data.template emplace<UnexpectedWrapper>(UnexpectedWrapper{ std::forward<TError>(error) });
    }

    /// Conversion to bool, checking whether object constains expected value.
    explicit operator bool() const {
        return isExpected();
    }

    /// Negation operator, returns true if object does NOT contain expected value.
    bool operator!() const {
        return !isExpected();
    }

    /// \brief Returns the reference to expected value.
    ///
    /// Object must not contain unexpected value, checked by assert.
    Type& value() {
        SPH_ASSERT(isExpected());
        return data.template get<Type>();
    }

    /// \brief Returns the const reference to expected value.
    ///
    /// Object must not contain unexpected value, checked by assert.
    const Type& value() const {
        SPH_ASSERT(isExpected());
        return data.template get<Type>();
    }

    /// \brief Returns the expected value or given alternative if the object contains unexpected value.
    Type valueOr(const Type& other) const {
        if (isExpected()) {
            return this->value();
        } else {
            return other;
        }
    }

    /// \brief Returns the error message.
    ///
    /// Object must contain unexpected value, checked by assert.
    const Error& error() const {
        SPH_ASSERT(!isExpected());
        return data.template get<UnexpectedWrapper>().error;
    }

    /// \brief Operator -> for convenient access to member variables and functions of expected value.
    ///
    /// If the object contains unexpected, throws an assert.
    Type* operator->() {
        SPH_ASSERT(isExpected());
        return &value();
    }

    /// \copydoc Type* operator->()
    const Type* operator->() const {
        SPH_ASSERT(isExpected());
        return &value();
    }

private:
    bool isExpected() const {
        return data.getTypeIdx() == 1;
    }
};

/// \brief Prints the expected value into a stream.
///
/// If the object contains an expected value, prints the value into the stream, otherwise print the error
/// message. Enabled only if the wrapped type defines the operator<<, to that it corretly works with Catch
/// framework.
template <typename T, typename = decltype(std::declval<std::ostream&>() << std::declval<T>())>
inline std::ostream& operator<<(std::ostream& stream, const Expected<T>& expected) {
    if (expected) {
        stream << expected.value();
    } else {
        stream << expected.error();
    }
    return stream;
}

/// \brief Constructs an unexpected value of given type, given error message as std::string.
///
/// For other type of error messages, use constructor of Expected.
template <typename Type>
Expected<Type> makeUnexpected(const std::string& error) {
    return Expected<Type>(UNEXPECTED, error);
}

NAMESPACE_SPH_END
