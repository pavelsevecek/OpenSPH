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
    FlatMap<String, ColorLut> luts;

    Project();

public:
    CallbackSet<void(const String& name, const ColorLut& lut)> onLutChanged;

    static Project& getInstance() {
        static Project project;
        return project;
    }

    Project clone() const {
        Project cloned;
        cloned.gui = gui;
        cloned.luts = luts.clone();
        return cloned;
    }

    void setColorLut(const String& name, const ColorLut& lut) {
        luts.insert(name, lut);
        onLutChanged(name, lut);
    }

    bool getColorLut(const String& name, ColorLut& lut) const {
        if (luts.contains(name)) {
            lut = luts[name];
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
    void setLuts(Config& config);
    void saveGui(Config& config);

    void loadLuts(Config& config);
    void loadGui(Config& config);
};

NAMESPACE_SPH_END
