#pragma once

#include "gui/ImageTransform.h"
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
        return interpolate(bitmap, uvw[X] * size.x, uvw[Y] * size.y);
    }
};

NAMESPACE_SPH_END
