#pragma once

#include "gui/objects/Colorizer.h"
#include "objects/containers/FlatMap.h"

NAMESPACE_SPH_BEGIN

class Config {
private:
    /// \brief User-specified palettes to be used instead of default values.
    FlatMap<ColorizerId, Palette> palettes;

public:
    static Config& getInstance() {
        static Config instance;
        return instance;
    }

    static bool getPalette(const ColorizerId id, Palette& palette);

    static bool setPalette(const ColorizerId id, const Palette& palette);
};

NAMESPACE_SPH_END
