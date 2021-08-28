#include "gui/Project.h"
#include "objects/utility/Streams.h"
#include "run/VirtualSettings.h"

NAMESPACE_SPH_BEGIN

Project::Project() {
    gui.set(GuiSettingsId::VIEW_WIDTH, 1024)
        .set(GuiSettingsId::VIEW_HEIGHT, 768)
        .set(GuiSettingsId::VIEW_MAX_FRAMERATE, 100)
        .set(GuiSettingsId::WINDOW_WIDTH, 1600)
        .set(GuiSettingsId::WINDOW_HEIGHT, 768)
        .set(GuiSettingsId::PARTICLE_RADIUS, 1._f)
        .set(GuiSettingsId::SURFACE_RESOLUTION, 1.e2_f)
        .set(GuiSettingsId::SURFACE_LEVEL, 0.13_f)
        .set(GuiSettingsId::SURFACE_AMBIENT, 0.1_f)
        .set(GuiSettingsId::SURFACE_SUN_POSITION, getNormalized(Vector(-0.4f, -0.1f, 0.6f)))
        .set(GuiSettingsId::RAYTRACE_ITERATION_LIMIT, 10)
        .set(GuiSettingsId::RAYTRACE_SUBSAMPLING, 4)
        .set(GuiSettingsId::CAMERA_TYPE, CameraEnum::ORTHO)
        .set(GuiSettingsId::CAMERA_ORTHO_FOV, 1.e5_f)
        .set(GuiSettingsId::CAMERA_ORTHO_CUTOFF, 0._f)
        .set(GuiSettingsId::PLOT_INITIAL_PERIOD, 60._f)
        .set(GuiSettingsId::PLOT_OVERPLOT_SFD, String("reference_sfd.txt"))
        .set(GuiSettingsId::PLOT_INTEGRALS,
            PlotEnum::KINETIC_ENERGY | PlotEnum::TOTAL_ENERGY | PlotEnum::INTERNAL_ENERGY |
                PlotEnum::TOTAL_ANGULAR_MOMENTUM | PlotEnum::TOTAL_MOMENTUM);
}

void Project::save(Config& config) {
    this->setLuts(config);
    this->saveGui(config);
}

void Project::load(Config& config) {
    this->loadLuts(config);
    this->loadGui(config);
}

void Project::reset() {
    *this = Project();
}

template <typename T>
INLINE auto settingsToConfigValue(const T& value) {
    return value;
}

template <>
INLINE auto settingsToConfigValue(const EnumWrapper& value) {
    return int(value.value);
}
template <>
INLINE auto settingsToConfigValue(const TracelessTensor& UNUSED(value)) {
    NOT_IMPLEMENTED;
    return 0;
}
template <>
INLINE auto settingsToConfigValue(const SymmetricTensor& UNUSED(value)) {
    NOT_IMPLEMENTED;
    return 0;
}
template <>
INLINE auto settingsToConfigValue(const Interval& UNUSED(value)) {
    NOT_IMPLEMENTED;
    return 0;
}

template <typename TOrig, typename TNew>
INLINE auto configToSettingsValue(const TOrig& UNUSED(original), const TNew& value) {
    return value;
}
template <>
INLINE auto configToSettingsValue(const EnumWrapper& original, const int& value) {
    EnumWrapper ew;
    ew.value = value;
    ew.index = original.index;
    return ew;
}


void Project::saveGui(Config& config) {
    SharedPtr<ConfigNode> guiNode = config.addNode("gui");
    for (const auto& entry : gui) {
        const Optional<String> key = GuiSettings::getEntryName(entry.id);
        if (!key) {
            throw ConfigException("No settings entry with id {}", int(entry.id));
        }
        forValue(entry.value, [&guiNode, &key](const auto& value) { //
            guiNode->set(key.value(), settingsToConfigValue(value));
        });
    }
}

void Project::setLuts(Config& config) {
    SharedPtr<ConfigNode> lutParentNode = config.addNode("palettes");
    for (auto& element : luts) {
        SharedPtr<ConfigNode> lutNode = lutParentNode->addChild(element.key());
        const ColorLut& lut = element.value();
        lutNode->set("lower", lut.getInterval().lower());
        lutNode->set("upper", lut.getInterval().upper());
        lutNode->set("scale", int(lut.getScale()));

        StringTextOutputStream ss;
        lut.getPalette().saveToStream(ss);
        String data = ss.toString();
        data.replaceAll("\n", ";");
        lutNode->set("data", data);
    }
}

void Project::loadGui(Config& config) {
    SharedPtr<ConfigNode> guiNode = config.getNode("gui");
    for (const auto& entry : gui) {
        const Optional<String> key = GuiSettings::getEntryName(entry.id);
        if (!key) {
            throw ConfigException("No settings entry with id {}", int(entry.id));
        }

        forValue(entry.value, [this, &guiNode, &key, &entry](auto& value) {
            using Type = decltype(settingsToConfigValue(value));

            try {
                const Type loadedValue = guiNode->get<Type>(key.value());
                gui.set(entry.id, configToSettingsValue(value, loadedValue));
            } catch (ConfigException& UNUSED(e)) {
                /// \todo log the error somewhere - we don't want to bail out here to allow loading older
                /// sessions
            }
        });
    }

    /// \todo more systematic solution
    Rgba background = gui.get<Rgba>(GuiSettingsId::BACKGROUND_COLOR);
    background.a() = 1.f;
    gui.set<Rgba>(GuiSettingsId::BACKGROUND_COLOR, background);

    if (gui.get<Float>(GuiSettingsId::CAMERA_ORTHO_FOV) == 0._f) {
        gui.set(GuiSettingsId::CAMERA_ORTHO_FOV, 1.e5_f);
        gui.set(GuiSettingsId::CAMERA_AUTOSETUP, true);
    }
}

void Project::loadLuts(Config& config) {
    luts.clear();
    SharedPtr<ConfigNode> lutParentNode = config.getNode("palettes");
    lutParentNode->enumerateChildren([this](const String& name, ConfigNode& lutNode) {
        const float lower = float(lutNode.get<Float>("lower"));
        const float upper = float(lutNode.get<Float>("upper"));
        const PaletteScale scale = PaletteScale(lutNode.get<int>("scale"));

        Palette palette;
        if (lutNode.contains("data")) {
            String data = lutNode.get<String>("data");
            data.replaceAll(";", "\n");
            StringTextInputStream ss(data);
            if (palette.loadFromStream(ss)) {
                luts.insert(name, ColorLut(palette, Interval(lower, upper), scale));
            }
        } else { // older format, palettes in separate files
            const String path = lutNode.get<String>("file");
            if (palette.loadCsvFromFile(Path(path))) {
                luts.insert(name, ColorLut(palette, Interval(lower, upper), scale));
            }
        }
    });
}

NAMESPACE_SPH_END
