#include "gui/objects/Bitmap.h"
#include "io/FileSystem.h"
#include "io/Path.h"
#include "objects/Exceptions.h"
#include <wx/bitmap.h>
#include <wx/log.h>
#include <wx/rawbmp.h>

NAMESPACE_SPH_BEGIN

void toWxBitmap(const Bitmap<Rgba>& bitmap, wxBitmap& wx) {
    const Pixel size = bitmap.size();
    if (!wx.IsOk() || wx.GetSize() != wxSize(size.x, size.y)) {
        wx.Create(size.x, size.y, 32);
    }
    ASSERT(wx.IsOk());

    wxAlphaPixelData pixels(wx);
    ASSERT(pixels);
    wxAlphaPixelData::Iterator iterator(pixels);
    ASSERT(iterator.IsOk());
    static_assert(
        std::is_same<std::decay_t<decltype(iterator.Red())>, unsigned char>::value, "expected unsigned char");

    for (int y = 0; y < bitmap.size().y; ++y) {
        for (int x = 0; x < bitmap.size().x; ++x) {
            const Rgba rgba = bitmap[Pixel(x, y)];
            wxColour color(rgba);
            iterator.Red() = color.Red();
            iterator.Green() = color.Green();
            iterator.Blue() = color.Blue();
            iterator.Alpha() = clamp(int(255.f * rgba.a()), 0, 255);

            ++iterator;
        }
    }
    ASSERT(wx.IsOk());
}

Bitmap<Rgba> toBitmap(wxBitmap& wx) {
    Bitmap<Rgba> bitmap(Pixel(wx.GetWidth(), wx.GetHeight()));

    wxNativePixelData pixels(wx);
    ASSERT(pixels);
    wxNativePixelData::Iterator iterator(pixels);
    ASSERT(iterator.IsOk());
    static_assert(
        std::is_same<std::decay_t<decltype(iterator.Red())>, unsigned char>::value, "expected unsigned char");

    for (int y = 0; y < bitmap.size().y; ++y) {
        for (int x = 0; x < bitmap.size().x; ++x) {
            bitmap[Pixel(x, y)] = Rgba(wxColour(iterator.Red(), iterator.Green(), iterator.Blue()));
            ++iterator;
        }
    }

    return bitmap;
}

void saveToFile(const wxBitmap& wx, const Path& path) {
    FileSystem::createDirectory(path.parentPath());
    wx.SaveFile(path.native().c_str(), wxBITMAP_TYPE_PNG);
}

void saveToFile(const Bitmap<Rgba>& bitmap, const Path& path) {
    wxBitmap wx;
    toWxBitmap(bitmap, wx);
    saveToFile(wx, path);
}

Bitmap<Rgba> loadBitmapFromFile(const Path& path) {
    wxLogNull logNullGuard; // we have custom error reporting
    wxBitmap wx;
    if (!wx.LoadFile(path.native().c_str())) {
        throw IoError("Cannot load bitmap '" + path.native() + "'");
    }

    if (!wx.IsOk()) {
        throw IoError("Bitmap '" + path.native() + "' failed to load correctly");
    }
    return toBitmap(wx);
}

NAMESPACE_SPH_END
