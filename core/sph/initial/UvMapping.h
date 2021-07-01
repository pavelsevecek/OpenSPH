#pragma once

#include "objects/geometry/Box.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

class IUvMapping : public Polymorphic {
public:
    virtual Array<Vector> generate(const Storage& storage) const = 0;
};

class SphericalUvMapping : public IUvMapping {
public:
    virtual Array<Vector> generate(const Storage& storage) const override {
        SPH_ASSERT(storage.getMaterialCnt() == 1);

        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        const Vector center = getCenterOfMass(storage);
        Array<Vector> uvws(r.size());

        for (Size i = 0; i < r.size(); ++i) {
            const Vector xyz = r[i] - center;
            SphericalCoords spherical = cartensianToSpherical(Vector(xyz[X], xyz[Z], xyz[Y]));
            uvws[i] = Vector(spherical.phi / (2._f * PI) + 0.5_f, spherical.theta / PI, 0._f);
            SPH_ASSERT(uvws[i][X] >= 0._f && uvws[i][X] <= 1._f, uvws[i][X]);
            SPH_ASSERT(uvws[i][Y] >= 0._f && uvws[i][Y] <= 1._f, uvws[i][Y]);
        }

        return uvws;
    }
};

class PlanarUvMapping : public IUvMapping {
public:
    virtual Array<Vector> generate(const Storage& storage) const override {
        SPH_ASSERT(storage.getMaterialCnt() == 1);

        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        const Box bbox = getBoundingBox(storage);
        Array<Vector> uvws(r.size());

        for (Size i = 0; i < r.size(); ++i) {
            const Vector xyz = (r[i] - bbox.lower()) / bbox.size();
            uvws[i] = Vector(xyz[0], xyz[1], 0._f);
            SPH_ASSERT(uvws[i][X] >= 0._f && uvws[i][X] <= 1._f, uvws[i][X]);
            SPH_ASSERT(uvws[i][Y] >= 0._f && uvws[i][Y] <= 1._f, uvws[i][Y]);
        }

        return uvws;
    }
};

NAMESPACE_SPH_END
