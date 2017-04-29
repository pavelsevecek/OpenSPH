#pragma once

/// Wrapper of wxBitmap, will be possibly replaced by custom implementation.
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "common/Assert.h"
#include "objects/Object.h"
#include <wx/bitmap.h>

NAMESPACE_SPH_BEGIN

class Bitmap {
private:
    wxBitmap impl;

public:
    Bitmap() = default;

    Bitmap(const wxBitmap& bitmap)
        : impl(bitmap) {}

    operator wxBitmap&() {
        ASSERT(impl.IsOk());
        return impl;
    }

    void saveToFile(const std::string& path) const {
        ASSERT(impl.IsOk());
        impl.SaveFile(path.c_str(), wxBITMAP_TYPE_PNG);
    }

    bool isOk() const {
        return impl.IsOk();
    }
};

NAMESPACE_SPH_END
