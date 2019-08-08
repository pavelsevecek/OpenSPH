#pragma once

#include "quantities/Quantity.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

class CompressedVector {
    StaticArray<float, 3> data;

public:
    CompressedVector() = default;

    CompressedVector(const Vector& v) {
        data[X] = v[X];
        data[Y] = v[Y];
        data[Z] = v[Z];
    }

    explicit operator Vector() const {
        return Vector(data[X], data[Y], data[Z]);
    }
};

/// \brief  contains reduced information
class CompressedStorage : public Noncopyable {
private:
    Array<CompressedVector> positions;
    Array<CompressedVector> velocities;
    Array<float> radii;

public:
    CompressedStorage() = default;

    CompressedStorage(const Storage& storage) {
        ArrayView<const Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);

        positions.resize(r.size());
        velocities.resize(r.size());
        radii.resize(r.size());

        for (Size i = 0; i < r.size(); ++i) {
            positions[i] = r[i];
            velocities[i] = v[i];
            radii[i] = r[i][H];
        }
    }

    explicit operator Storage() const {
        Storage storage;
        Array<Vector> r(positions.size());
        for (Size i = 0; i < r.size(); ++i) {
            r[i] = Vector(positions[i]);
            r[i][H] = radii[i];
        }
        storage.insert<Vector>(QuantityId::POSITION, OrderEnum::FIRST, std::move(r));

        Array<Vector> v(velocities.size());
        for (Size i = 0; i < v.size(); ++i) {
            v[i] = Vector(velocities[i]);
        }
        storage.getDt<Vector>(QuantityId::POSITION) = std::move(v);

        return storage;
    }
};

NAMESPACE_SPH_END
