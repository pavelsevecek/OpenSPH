#pragma once

#include "gui/objects/Colorizer.h"
#include "objects/containers/FlatMap.h"

NAMESPACE_SPH_BEGIN

class Config {
private:
    /// \brief User-specified palettes to be used instead of default values.
    FlatMap<std::string, Palette> paletteMap;

public:
    Config();

    static Config& getInstance() {
        static Config instance;
        return instance;
    }

    static bool getPalette(const std::string& name, Palette& palette);

    static bool setPalette(const std::string& name, const Palette& palette);

private:
    Path getConfigPath();

    void save();

    void load();
};

NAMESPACE_SPH_END
