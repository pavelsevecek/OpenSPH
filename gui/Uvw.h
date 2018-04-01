#pragma once

#include "gui/objects/Bitmap.h"
#include "quantities/IMaterial.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

enum class GuiQuantityId {
    UVW = 1000,
};

enum TextureFiltering {
    NEAREST_NEIGHBOUR,
    BILINEAR,
};

class Texture {
private:
    Bitmap bitmap;
    TextureFiltering filtering;

public:
    explicit Texture(Bitmap&& bitmap, const TextureFiltering filtering)
        : bitmap(std::move(bitmap))
        , filtering(filtering) {}

    explicit Texture(const Path& path, const TextureFiltering filtering)
        : filtering(std::move(filtering)) {
        bitmap = loadBitmapFromFile(path);
    }

    Color eval(const Vector& uvw) const {
        switch (filtering) {
        case TextureFiltering::NEAREST_NEIGHBOUR:
            return this->evalNearestNeighbour(uvw);
        case TextureFiltering::BILINEAR:
            return this->evalBilinear(uvw);
        default:
            NOT_IMPLEMENTED;
        }
    }

private:
    Color evalNearestNeighbour(const Vector& uvw) const {
        const Point size = bitmap.size();
        const Size u = clamp(int(uvw[X] * size.x), 0, size.x - 1);
        const Size v = clamp(int(uvw[Y] * size.y), 0, size.y - 1);
        return Color(bitmap[Point(u, v)]);
    }

    Color evalBilinear(const Vector& uvw) const {
        const Point size = bitmap.size();
        const Vector textureUvw = clamp(Vector(uvw[X] * size.x, uvw[Y] * size.y, 0._f),
            Vector(0._f),
            Vector(size.x - 1, size.y - 1, 0._f) - Vector(EPS));
        const Size u1 = int(textureUvw[X]);
        const Size v1 = int(textureUvw[Y]);
        const Size u2 = u1 + 1;
        const Size v2 = v1 + 1;
        const Float a = textureUvw[X] - u1;
        const Float b = textureUvw[Y] - v1;
        ASSERT(a >= 0._f && a < 1._f, a);
        ASSERT(b >= 0._f && b < 1._f, b);

        return Color(bitmap[Point(u1, v1)]) * (1._f - a) * (1._f - b) +
               Color(bitmap[Point(u2, v1)]) * a * (1._f - b) + //
               Color(bitmap[Point(u1, v2)]) * (1._f - a) * b + //
               Color(bitmap[Point(u2, v2)]) * a * b;
    }
};

inline void setupUvws(Storage& storage) {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    Array<Vector> uvws(r.size());
    for (Size matId = 0; matId < storage.getMaterialCnt(); ++matId) {
        MaterialView mat = storage.getMaterial(matId);
        const Vector center = mat->getParam<Vector>(BodySettingsId::BODY_CENTER);
        for (Size i : mat.sequence()) {
            const Vector xyz = r[i] - center;
            SphericalCoords spherical = cartensianToSpherical(Vector(xyz[X], xyz[Z], xyz[Y]));
            uvws[i] = Vector(spherical.phi / (2._f * PI) + 0.5_f, spherical.theta / PI, 0._f);
            ASSERT(uvws[i][X] >= 0._f && uvws[i][X] <= 1._f, uvws[i][X]);
            ASSERT(uvws[i][Y] >= 0._f && uvws[i][Y] <= 1._f, uvws[i][Y]);
        }
    }
    storage.insert<Vector>(QuantityId(GuiQuantityId::UVW), OrderEnum::ZERO, std::move(uvws));
}

NAMESPACE_SPH_END
