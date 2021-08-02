#pragma once

#include "gui/objects/Bitmap.h"
#include "quantities/IMaterial.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

enum TextureFiltering {
    NEAREST_NEIGHBOR,
    BILINEAR,
};

class Texture : public Noncopyable {
private:
    Bitmap<Rgba> bitmap;
    TextureFiltering filtering;

public:
    Texture() = default;

    explicit Texture(Bitmap<Rgba>&& bitmap, const TextureFiltering filtering)
        : bitmap(std::move(bitmap))
        , filtering(filtering) {}

    explicit Texture(const Path& path, const TextureFiltering filtering)
        : filtering(std::move(filtering)) {
        bitmap = loadBitmapFromFile(path);
    }

    Rgba eval(const Vector& uvw) const {
        switch (filtering) {
        case TextureFiltering::NEAREST_NEIGHBOR:
            return this->evalNearestNeighbor(uvw);
        case TextureFiltering::BILINEAR:
            return this->evalBilinear(uvw);
        default:
            NOT_IMPLEMENTED;
        }
    }

    Texture clone() const {
        Texture cloned;
        cloned.filtering = filtering;
        cloned.bitmap = Bitmap<Rgba>(bitmap.size());
        for (int y = 0; y < bitmap.size().y; ++y) {
            for (int x = 0; x < bitmap.size().x; ++x) {
                cloned.bitmap[Pixel(x, y)] = bitmap[Pixel(x, y)];
            }
        }
        return cloned;
    }

    bool empty() const {
        return bitmap.empty();
    }

private:
    Rgba evalNearestNeighbor(const Vector& uvw) const {
        const Pixel size = bitmap.size();
        const Size u = clamp(int(uvw[X] * size.x), 0, size.x - 1);
        const Size v = clamp(int(uvw[Y] * size.y), 0, size.y - 1);
        return Rgba(bitmap[Pixel(u, v)]);
    }

    Rgba evalBilinear(const Vector& uvw) const {
        const Pixel size = bitmap.size();
        const Vector textureUvw = clamp(Vector(uvw[X] * size.x, uvw[Y] * size.y, 0._f),
            Vector(0._f),
            Vector(size.x - 1, size.y - 1, 0._f) - Vector(EPS));
        const Size u1 = int(textureUvw[X]);
        const Size v1 = int(textureUvw[Y]);
        const Size u2 = u1 + 1;
        const Size v2 = v1 + 1;
        const float a = float(textureUvw[X] - u1);
        const float b = float(textureUvw[Y] - v1);
        SPH_ASSERT(a >= 0.f && a <= 1.f, a);
        SPH_ASSERT(b >= 0.f && b <= 1.f, b);

        return Rgba(bitmap[Pixel(u1, v1)]) * (1.f - a) * (1.f - b) +
               Rgba(bitmap[Pixel(u2, v1)]) * a * (1.f - b) + //
               Rgba(bitmap[Pixel(u1, v2)]) * (1.f - a) * b + //
               Rgba(bitmap[Pixel(u2, v2)]) * a * b;
    }
};

NAMESPACE_SPH_END
