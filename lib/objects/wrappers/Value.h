#pragma once

#include "geometry/TracelessTensor.h"
#include "math/Means.h"
#include "objects/wrappers/Variant.h"
#include "system/Logger.h"

/// Convenient object for storing a single value of different types: scalar, vector, tensor, means or integer.
/// This is indended mainly for logging and output routines, as the object provides generic way to store
/// different types and print stored values. There is no need no need to pass types as template arguments, so
/// the object Value is a suitable return value or parameter of virtual functions.
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

NAMESPACE_SPH_BEGIN

class Value {
private:
    Variant<Size, Float, Vector, Tensor, TracelessTensor, Means> value;

public:
    Value() = default;

    template <typename T>
    Value(T&& value)
        : value(std::forward<T>(value)) {}

    template <typename T>
    Value& operator=(T&& rhs) {
        value = std::forward<T>(rhs);
        return *this;
    }


    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const Value& value) {
        forValue(value, [&stream](auto& v) { stream << v; });
    }
};


NAMESPACE_SPH_END
