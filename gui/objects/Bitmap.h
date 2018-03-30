#pragma once

/// \file Bitmap.h
/// \brief Wrapper of wxBitmap, will be possibly replaced by custom implementation.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "gui/objects/Color.h"
#include "gui/objects/Point.h"
#include "io/Path.h"
#include <wx/bitmap.h>
#include <wx/rawbmp.h>

NAMESPACE_SPH_BEGIN

class Bitmap : public Noncopyable {
private:
    wxBitmap impl;

public:
    Bitmap() = default;

    Bitmap(const Point res)
        : impl(res.x, res.y) {}

    Bitmap(const wxBitmap& bitmap)
        : impl(bitmap) {}

    Point size() const {
        return Point(impl.GetWidth(), impl.GetHeight());
    }

    operator wxBitmap&() {
        ASSERT(impl.IsOk());
        return impl;
    }

    void saveToFile(const Path& path) const {
        ASSERT(impl.IsOk());
        impl.SaveFile(path.native().c_str(), wxBITMAP_TYPE_PNG);
    }

    bool isOk() const {
        return impl.IsOk();
    }
};

class BitmapIterator {
private:
    wxNativePixelData pixels;
    wxNativePixelData::Iterator iterator;

public:
    explicit BitmapIterator(Bitmap& bitmap)
        : pixels(bitmap)
        , iterator(pixels) {
        ASSERT(pixels);
        ASSERT(iterator.IsOk());

        static_assert(std::is_same<std::decay_t<decltype(iterator.Red())>, unsigned char>::value,
            "expected unsigned char");
    }

    BitmapIterator& operator++() {
        ++iterator;
        return *this;
    }

    Color getColor() const {
        wxNativePixelData::Iterator& i = const_cast<wxNativePixelData::Iterator&>(iterator);
        return Color(wxColour(i.Red(), i.Green(), i.Blue()));
    }

    float getAlpha() const {
        wxNativePixelData::Iterator& i = const_cast<wxNativePixelData::Iterator&>(iterator);
        return i.Alpha() / 255.f;
    }

    void setColor(const Color& color) {
        wxColour wxcolor(color);
        iterator.Red() = wxcolor.Red();
        iterator.Green() = wxcolor.Green();
        iterator.Blue() = wxcolor.Blue();
    }

    void setAlpha(const float alpha) {
        iterator.Alpha() = alpha * 255.f;
    }
};


NAMESPACE_SPH_END
