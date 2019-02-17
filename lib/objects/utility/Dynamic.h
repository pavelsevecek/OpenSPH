#pragma once

/// \file Dynamic.h
/// \brief Object holding a single values of various types
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "math/Means.h"
#include "objects/geometry/Tensor.h"
#include "objects/geometry/TracelessTensor.h"
#include "objects/wrappers/Variant.h"

NAMESPACE_SPH_BEGIN

/// \brief Enum representing a value type stored in a Value object.
///
/// Has to be kept in sync with ValueVariant defined below.
enum class DynamicId {
    SIZE = 1,
    FLOAT,
    VECTOR,
    TENSOR,
    SYMMETRIC_TENSOR,
    TRACELESS_TENSOR,
    MIN_MAX_MEAN,
};

/// \brief Convenient object for storing a single value of different types
///
/// Object can currently store scalar, vector, tensor, unsigned integer and \ref MinMaxMean.
/// This is intended mainly for logging and output routines, as the object provides generic way to store
/// different types and print stored values. There is no need to pass types as template arguments, so the
/// object Value is a suitable return value or parameter of virtual functions (that cannot be templated).
class Dynamic {
private:
    /// \note If bool is added into the variant, it will probably clash with operator bool().
    using DynamicVariant = Variant<NothingType,
        Size,
        Float,
        Vector,
        Tensor,
        SymmetricTensor,
        TracelessTensor,
        MinMaxMean,
        std::string>;

    DynamicVariant storage;

public:
    /// Construct an uninitialized value
    Dynamic();

    ~Dynamic();

    /// Contruct value from one of possible value types
    template <typename T, typename = std::enable_if_t<!std::is_same<std::decay_t<T>, Dynamic>::value>>
    Dynamic(T&& value)
        : storage(std::forward<T>(value)) {}

    Dynamic(const Dynamic& other)
        : storage(other.storage) {}

    Dynamic(Dynamic&& other)
        : storage(std::move(other.storage)) {}

    /// Assign one of possible value types into the value
    template <typename T, typename = std::enable_if_t<DynamicVariant::canHold<T>()>>
    Dynamic& operator=(T&& rhs) {
        storage = std::forward<T>(rhs);
        return *this;
    }

    Dynamic& operator=(const Dynamic& other) {
        storage = other.storage;
        return *this;
    }

    Dynamic& operator=(Dynamic&& other) {
        storage = std::move(other.storage);
        return *this;
    }

    /// \brief Returns the reference to the stored value given its type.
    ///
    /// The stored value must indeed have a type T, checked by assert.
    template <typename T, typename = std::enable_if_t<DynamicVariant::canHold<T>()>>
    INLINE T& get() {
        return storage.get<T>();
    }

    /// Returns the const reference to the stored value given its type.
    template <typename T, typename = std::enable_if_t<DynamicVariant::canHold<T>()>>
    const T& get() const {
        return storage.get<T>();
    }

    /// Conversion operator to one of stored types
    template <typename T, typename = std::enable_if_t<DynamicVariant::canHold<T>()>>
    operator T&() {
        return this->get<T>();
    }

    /// Coversion operator to one of stored types, const version
    template <typename T, typename = std::enable_if_t<DynamicVariant::canHold<T>()>>
    operator const T&() const {
        return this->get<T>();
    }

    /// Converts the stored value into a single number, using one of possible conversions
    Float getScalar() const {
        return forValue(storage, ScalarFunctor());
    }

    DynamicId getType() const {
        return DynamicId(storage.getTypeIdx());
    }

    /// Checks if the value has been initialized.
    bool empty() const {
        return storage.getTypeIdx() == 0;
    }

    explicit operator bool() const {
        return !this->empty();
    }

    bool operator!() const {
        return this->empty();
    }

    /// Equality operator with one of stored types.
    template <typename T, typename = std::enable_if_t<DynamicVariant::canHold<T>()>>
    bool operator==(const T& value) const {
        return this->get<T>() == value;
    }

    /// Prints the currently stored value into the stream, using << operator of its type.
    friend std::ostream& operator<<(std::ostream& stream, const Dynamic& value) {
        forValue(value.storage, [&stream](const auto& v) { stream << std::setw(20) << v; });
        return stream;
    }

private:
    struct ScalarFunctor {
        template <typename T>
        Float operator()(const T& value) {
            return norm(value);
        }
        Float operator()(const Size value) {
            return Float(value);
        }
        Float operator()(const Vector& value) {
            return getLength(value);
        }
        Float operator()(const MinMaxMean& value) {
            return value.mean();
        }
        Float operator()(const std::string&) {
            NOT_IMPLEMENTED;
        }
        Float operator()(const NothingType&) {
            return NAN;
        }
    };
};


NAMESPACE_SPH_END
