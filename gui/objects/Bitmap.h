#pragma once

/// \file Bitmap.h
/// \brief Wrapper of wxBitmap, will be possibly replaced by custom implementation.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "gui/objects/Color.h"
#include "gui/objects/Point.h"
#include "io/FileSystem.h"
#include "io/Path.h"
#include "objects/containers/Array.h"
#include <wx/bitmap.h>
#include <wx/rawbmp.h>

NAMESPACE_SPH_BEGIN

class Bitmap : public Noncopyable {
private:
    Array<wxColour> values;
    Point res;

public:
    Bitmap()
        : res(0, 0) {}

    explicit Bitmap(const Point res)
        : res(res) {
        values.resize(res.x * res.y);
    }

    wxColour& operator[](const Point p) {
        return values[map(p)];
    }

    const wxColour& operator[](const Point p) const {
        return values[map(p)];
    }

    Point size() const {
        return res;
    }

    bool empty() const {
        return values.empty();
    }

private:
    Size map(const Point p) const {
        return p.y * res.x + p.x;
    }
};


inline void toWxBitmap(const Bitmap& bitmap, wxBitmap& wx) {
    wx.Create(bitmap.size().x, bitmap.size().y);
    ASSERT(wx.IsOk());

    wxNativePixelData pixels(wx);
    ASSERT(pixels);
    wxNativePixelData::Iterator iterator(pixels);
    ASSERT(iterator.IsOk());
    static_assert(
        std::is_same<std::decay_t<decltype(iterator.Red())>, unsigned char>::value, "expected unsigned char");

    for (int y = 0; y < bitmap.size().y; ++y) {
        for (int x = 0; x < bitmap.size().x; ++x) {
            wxColour color = bitmap[Point(x, y)];
            iterator.Red() = color.Red();
            iterator.Green() = color.Green();
            iterator.Blue() = color.Blue();

            ++iterator;
        }
    }
    ASSERT(wx.IsOk());
}

inline Bitmap toBitmap(wxBitmap& wx) {
    Bitmap bitmap(Point(wx.GetWidth(), wx.GetHeight()));

    wxNativePixelData pixels(wx);
    ASSERT(pixels);
    wxNativePixelData::Iterator iterator(pixels);
    ASSERT(iterator.IsOk());
    static_assert(
        std::is_same<std::decay_t<decltype(iterator.Red())>, unsigned char>::value, "expected unsigned char");

    for (int y = 0; y < bitmap.size().y; ++y) {
        for (int x = 0; x < bitmap.size().x; ++x) {
            bitmap[Point(x, y)] = wxColour(iterator.Red(), iterator.Green(), iterator.Blue());
            ++iterator;
        }
    }

    return bitmap;
}

inline void saveToFile(const wxBitmap& wx, const Path& path) {
    FileSystem::createDirectory(path.parentPath());
    wx.SaveFile(path.native().c_str(), wxBITMAP_TYPE_PNG);
}

inline void saveToFile(const Bitmap& bitmap, const Path& path) {
    wxBitmap wx;
    toWxBitmap(bitmap, wx);
    saveToFile(wx, path);
}

inline Bitmap loadBitmapFromFile(const Path& path) {
    wxBitmap wx;
    if (!wx.LoadFile(path.native().c_str())) {
        ASSERT(false, "Cannot load bitmap");
    }
    ASSERT(wx.IsOk());
    return toBitmap(wx);
}

NAMESPACE_SPH_END
