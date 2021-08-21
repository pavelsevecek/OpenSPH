#include "objects/utility/Dynamic.h"

NAMESPACE_SPH_BEGIN

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
    Float operator()(const String&) {
        NOT_IMPLEMENTED;
    }
    Float operator()(const NothingType&) {
        return NAN;
    }
};

Dynamic::Dynamic() = default;

/// Needs to be in .cpp to compile with clang, for some reason
Dynamic::~Dynamic() = default;

Float Dynamic::getScalar() const {
    return forValue(storage, ScalarFunctor());
}

NAMESPACE_SPH_END
