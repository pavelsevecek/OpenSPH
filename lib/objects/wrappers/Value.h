#pragma once

#include "geometry/TracelessTensor.h"
#include "math/Means.h"
#include "objects/wrappers/Variant.h"

/// Convenient object for storing a single value of different types: scalar, vector, tensor, means or integer.
/// This is intended mainly for logging and output routines, as the object provides generic way to store
/// different types and print stored values. There is no need to pass types as template arguments, so the
/// object Value is a suitable return value or parameter of virtual functions (that cannot be templated).
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

NAMESPACE_SPH_BEGIN

class Value {
private:
    Variant<Size, Float, Vector, Tensor, TracelessTensor, Means> storage;

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

    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const Value& value) {
        forValue(value.storage, [&stream](const auto& v) { stream << std::setw(20) << v; });
        return stream;
    }
};


NAMESPACE_SPH_END
