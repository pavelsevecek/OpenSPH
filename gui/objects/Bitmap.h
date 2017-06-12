#pragma once

/// \file Bitmap.h
/// \brief Wrapper of wxBitmap, will be possibly replaced by custom implementation.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/Assert.h"
#include "io/Path.h"
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

    void saveToFile(const Path& path) const {
        ASSERT(impl.IsOk());
        impl.SaveFile(path.native().c_str(), wxBITMAP_TYPE_PNG);
    }

    bool isOk() const {
        return impl.IsOk();
    }
};

NAMESPACE_SPH_END
