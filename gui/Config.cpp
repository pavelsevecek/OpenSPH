#include "gui/Config.h"
#include "io/FileSystem.h"
#include "objects/utility/StringUtils.h"
#include "system/Platform.h"
#include <fstream>

NAMESPACE_SPH_BEGIN

static Path getFileName(const std::string& name) {
    std::string s = name;
    for (Size i = 0; i < s.size(); ++i) {
        if (std::isalnum(s[i])) {
            s[i] = std::tolower(s[i]);
        } else {
            s[i] = '_';
        }
    }
    return Path(s + ".csv");
}

Config::Config() {
    this->load();
}

bool Config::getPalette(const std::string& name, Palette& palette) {
    Config& instance = getInstance();
    if (auto overridenPalette = instance.paletteMap.tryGet(name)) {
        palette = overridenPalette.value();
        return true;
    } else {
        const Path path = getFileName(name);
        if (FileSystem::pathExists(path)) {
            Outcome result = palette.loadFromFile(path);
            if (result) {
                instance.paletteMap.insert(name, palette);
                return true;
            }
        }
        return false;
    }
}

bool Config::setPalette(const std::string& name, const Palette& palette) {
    Config& instance = getInstance();
    instance.paletteMap.insert(name, palette);

    instance.save();
    return bool(palette.saveToFile(getFileName(name)));
}

Path Config::getConfigPath() {
    Expected<Path> executablePath = getExecutablePath();
    if (executablePath && FileSystem::isPathWritable(executablePath.value())) {
        return executablePath.value() / Path("palettes.conf");
    }
    Expected<Path> homePath = FileSystem::getHomeDirectory();
    if (homePath) {
        return executablePath.value() / Path(".config/opensph/palettes.conf");
    }
    return Path("palettes.conf");
}

void Config::save() {
    try {
        const Path path = getConfigPath();
        FileSystem::createDirectory(path.parentPath());
        std::ofstream ofs(path.native());
        for (FlatMap<std::string, Palette>::Element& e : paletteMap) {
            ofs << e.key << ": " << e.value.getInterval() << " " << std::setw(20) << int(e.value.getScale())
                << " " << std::setw(20) << getFileName(e.key) << std::endl;
        }
    } catch (...) {
    }
}

void Config::load() {
    try {
        paletteMap.clear();
        const Path path = getConfigPath();
        std::ifstream ifs(path.native());
        std::string line;
        while (std::getline(ifs, line)) {
            std::stringstream ss(line);
            std::string name;
            float lower, upper;
            int scale;
            std::string path;
            std::getline(ss, name, ':');
            ss >> lower >> upper >> scale >> path;
            Palette palette({ { lower, Rgba::black() }, { upper, Rgba::white() } }, PaletteScale(scale));
            Outcome result = palette.loadFromFile(Path(path));
            if (result) {
                paletteMap.insert(name, std::move(palette));
            }
        }
    } catch (...) {
    }
}

NAMESPACE_SPH_END
