#pragma once

/// Wrapper of wxBitmap, will be possibly replaced by custom implementation.
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"
#include <wx/bitmap.h>

NAMESPACE_SPH_BEGIN

class Bitmap {
private:
    wxBitmap impl;

public:
    Bitmap(const wxBitmap& bitmap)
        : impl(bitmap) {}

    void saveToFile(const std::string& path) const {
        impl.SaveFile(path, wxBITMAP_TYPE_PNG);
    }
};

NAMESPACE_SPH_END
