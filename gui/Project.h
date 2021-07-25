#pragma once

#include "gui/Settings.h"
#include "gui/objects/Palette.h"
#include "objects/containers/CallbackSet.h"
#include "run/Config.h"
#include "thread/CheckFunction.h"

NAMESPACE_SPH_BEGIN


/// \todo deduplicate
inline std::string getIdentifier(const std::string& name) {
    std::string escaped = replaceAll(name, " ", "-");
    return lowercase(escaped);
}

class Project {
private:
    GuiSettings gui;
    FlatMap<std::string, Palette> palettes;

    Project();

public:
    CallbackSet<void(const std::string& name, const Palette& palette)> onPaletteChanged;

    static Project& getInstance() {
        static Project project;
        return project;
    }

    Project clone() const {
        Project cloned;
        cloned.gui = gui;
        cloned.palettes = palettes.clone();
        return cloned;
    }

    void setPalette(const std::string& name, const Palette& palette) {
        palettes.insert(name, palette);

        onPaletteChanged(name, palette);
    }

    bool getPalette(const std::string& name, Palette& palette) const {
        if (palettes.contains(name)) {
            palette = palettes[name];
            return true;
        } else {
            return false;
        }
    }

    /// \todo synchronize
    GuiSettings& getGuiSettings() {
        return gui;
    }

    const GuiSettings& getGuiSettings() const {
        return gui;
    }

    void save(Config& config);

    void load(Config& config);

private:
    void savePalettes(Config& config);
    void saveGui(Config& config);

    void loadPalettes(Config& config);
    void loadGui(Config& config);
};

NAMESPACE_SPH_END
