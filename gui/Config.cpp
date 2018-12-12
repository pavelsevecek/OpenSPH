#include "gui/Config.h"
#include "objects/utility/StringUtils.h"
#include <fstream>

NAMESPACE_SPH_BEGIN

static Path getFileName(const ColorizerId id) {
    std::string s;
    if (int(id) >= 0) {
        s = getMetadata(QuantityId(id)).quantityName;
    } else {
        /// \todo
        s = std::to_string(int(id));
    }
    for (Size i = 0; i < s.size(); ++i) {
        if (std::isalnum(s[i])) {
            s[i] = std::tolower(s[i]);
        } else {
            s[i] = '_';
        }
    }
    return Path(s + ".csv");
}

bool Config::getPalette(const ColorizerId id, Palette& palette) {
    Config& instance = getInstance();
    if (auto overridenPalette = instance.palettes.tryGet(id)) {
        palette = overridenPalette.value();
        return true;
    } else {
        const Path path = getFileName(id);
        if (FileSystem::pathExists(path)) {
            Outcome result = palette.loadFromFile(path);
            if (result) {
                instance.palettes.insert(id, palette);
                return true;
            }
        }
        return false;
    }
}

bool Config::setPalette(const ColorizerId id, const Palette& palette) {
    Config& instance = getInstance();
    instance.palettes.insert(id, palette);

    // FileSystem::createDirectory(configDir);
    return bool(palette.saveToFile(getFileName(id)));
}


NAMESPACE_SPH_END
