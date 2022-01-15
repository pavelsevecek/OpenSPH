#pragma once

#include "gui/objects/Palette.h"
#include "run/VirtualSettings.h"

NAMESPACE_SPH_BEGIN

class PaletteEntry : public IExtraEntry {
private:
    Palette palette;

public:
    PaletteEntry() = default;

    PaletteEntry(const Palette& palette)
        : palette(palette) {}

    virtual String toString() const override;

    virtual void fromString(const String& s) override;

    virtual AutoPtr<IExtraEntry> clone() const override {
        return makeAuto<PaletteEntry>(palette);
    }

    void setPalette(const Palette& newPalette) {
        palette = newPalette;
    }

    const Palette& getPalette() const {
        return palette;
    }
};

NAMESPACE_SPH_END
