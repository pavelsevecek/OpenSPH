#pragma once

#include "gui/Settings.h"
#include "gui/objects/Palette.h"
#include "objects/containers/CallbackSet.h"
#include "run/Config.h"
#include "thread/CheckFunction.h"

NAMESPACE_SPH_BEGIN


/// \todo deduplicate
inline String getIdentifier(const String& name) {
    String identifier = name;
    identifier.replaceAll(" ", "-");
    return identifier.toLowercase();
}

class Project {
private:
    GuiSettings gui;
    FlatMap<String, Palette> palettes;

    Project();

public:
    CallbackSet<void(const String& name, const Palette& palette)> onPaletteChanged;

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

    void setPalette(const String& name, const Palette& palette) {
        palettes.insert(name, palette);

        onPaletteChanged(name, palette);
    }

    bool getPalette(const String& name, Palette& palette) const {
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

    void reset();

private:
    void savePalettes(Config& config);
    void saveGui(Config& config);

    void loadPalettes(Config& config);
    void loadGui(Config& config);
};

NAMESPACE_SPH_END
