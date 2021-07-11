#pragma once

/// \file Bitmap.h
/// \brief Wrapper of wxBitmap, will be possibly replaced by custom implementation.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "gui/objects/Color.h"
#include "gui/objects/Point.h"
#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

class Path;

template <typename Type>
class Bitmap : public Noncopyable {
private:
    Array<Type> values;
    Pixel res;

public:
    Bitmap()
        : res(0, 0) {}

    explicit Bitmap(const Pixel resolution)
        : res(resolution) {
        values.resize(res.x * res.y);
    }

    Bitmap clone() const {
        Bitmap cloned;
        cloned.values = values.clone();
        cloned.res = res;
        return cloned;
    }

    void resize(const Pixel newResolution, const Type& value) {
        res = newResolution;
        values.resize(res.x * res.y);
        this->fill(value);
    }

    void fill(const Type& value) {
        values.fill(value);
    }

    Type& operator[](const Pixel p) {
        return values[map(p)];
    }

    const Type& operator[](const Pixel p) const {
        return values[map(p)];
    }

    Type& operator()(const int x, const int y) {
        return values[map(Pixel(x, y))];
    }

    const Type& operator()(const int x, const int y) const {
        return values[map(Pixel(x, y))];
    }

    Pixel size() const {
        return res;
    }

    bool empty() const {
        return values.empty();
    }

private:
    INLINE Size map(const Pixel p) const {
        return p.y * res.x + p.x;
    }
};

void toWxBitmap(const Bitmap<Rgba>& bitmap, wxBitmap& wx);

Bitmap<Rgba> toBitmap(wxBitmap& wx);

void saveToFile(const wxBitmap& wx, const Path& path);

void saveToFile(const Bitmap<Rgba>& bitmap, const Path& path);

Bitmap<Rgba> loadBitmapFromFile(const Path& path);

NAMESPACE_SPH_END
