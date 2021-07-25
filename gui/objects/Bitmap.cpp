#include "gui/objects/Bitmap.h"
#include "io/FileSystem.h"
#include "io/Path.h"
#include "objects/Exceptions.h"
#include "thread/Scheduler.h"
#include <wx/bitmap.h>
#include <wx/log.h>
#include <wx/rawbmp.h>

NAMESPACE_SPH_BEGIN

void toWxBitmap(const Bitmap<Rgba>& bitmap, wxBitmap& wx, const float scale) {
    const Pixel size(Coords(bitmap.size()) / scale);
    if (!wx.IsOk() || wx.GetSize() != wxSize(size.x, size.y)) {
        wx.Create(size.x, size.y, 32);
    }
    SPH_ASSERT(wx.IsOk());

    wxAlphaPixelData pixels(wx);
    SPH_ASSERT(pixels);

    wxAlphaPixelData::Iterator iterator(pixels);
    SPH_ASSERT(iterator.IsOk());
    for (float y = 0; y < size.y; ++y) {
        for (float x = 0; x < size.x; ++x) {
            const int ix = min<int>(round(x * scale), bitmap.size().x - 1);
            const int iy = min<int>(round(y * scale), bitmap.size().y - 1);
            const Rgba rgba = bitmap[Pixel(ix, iy)];
            wxColour color(rgba);
            iterator.Red() = color.Red();
            iterator.Green() = color.Green();
            iterator.Blue() = color.Blue();
            iterator.Alpha() = clamp(int(255.f * rgba.a()), 0, 255);

            ++iterator;
        }
    }
    SPH_ASSERT(wx.IsOk());
}

Bitmap<Rgba> toBitmap(wxBitmap& wx) {
    Bitmap<Rgba> bitmap(Pixel(wx.GetWidth(), wx.GetHeight()));

    wxNativePixelData pixels(wx);
    SPH_ASSERT(pixels);
    wxNativePixelData::Iterator iterator(pixels);
    SPH_ASSERT(iterator.IsOk());
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
