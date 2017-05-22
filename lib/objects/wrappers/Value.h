#pragma once

/// \file Value.h
/// \brief Object holding a single values of various types
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "geometry/TracelessTensor.h"
#include "math/Means.h"
#include "objects/wrappers/Variant.h"

NAMESPACE_SPH_BEGIN

/// \brief Convenient object for storing a single value of different types
///
/// Object can currently store scalar, vector, tensor, means and integer.
/// This is intended mainly for logging and output routines, as the object provides generic way to store
/// different types and print stored values. There is no need to pass types as template arguments, so the
/// object Value is a suitable return value or parameter of virtual functions (that cannot be templated).
class Value {
private:
    Variant<Size, Float, Vector, SymmetricTensor, TracelessTensor, MinMaxMean> storage;

public:
    Value() = default;

    template <typename T>
    Value(T&& value)
        : storage(std::forward<T>(value)) {}

    template <typename T>
    Value& operator=(T&& rhs) {
        storage = std::forward<T>(rhs);
        return *this;
    }

    template <typename T>
    T& get() {
        return storage.get<T>();
    }

    template <typename T>
    const T& get() const {
        return storage.get<T>();
    }

    friend std::ostream& operator<<(std::ostream& stream, const Value& value) {
        forValue(value.storage, [&stream](const auto& v) { stream << std::setw(20) << v; });
        return stream;
    }
};


NAMESPACE_SPH_END
