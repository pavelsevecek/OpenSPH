#include "io/FileSystem.h"
#include "objects/Exceptions.h"
#include "objects/utility/StringUtils.h"
#include "objects/wrappers/Outcome.h"
#include "system/Settings.h"
#include <fstream>
#include <regex>

/// Settigs implementation, needs to be included in cpp with explicit instantiation of Settings.

NAMESPACE_SPH_BEGIN

template <typename TEnum>
Settings<TEnum>::Settings(std::initializer_list<Entry> list) {
    for (auto& entry : list) {
        ASSERT(!entries.contains(entry.id), "Duplicate settings ID ", int(entry.id));
        entries.insert(entry.id, entry);
    }
}

template <typename TEnum>
Settings<TEnum>::Settings()
    : Settings(Settings::getDefaults()) {}

template <typename TEnum>
Settings<TEnum>::Settings(EmptySettingsTag) {}

template <typename TEnum>
Settings<TEnum>::Settings(const Settings& other)
    : entries(other.entries.clone()) {}

template <typename TEnum>
Settings<TEnum>::Settings(Settings&& other)
    : entries(std::move(other.entries)) {}

template <typename TEnum>
Settings<TEnum>& Settings<TEnum>::operator=(std::initializer_list<Entry> list) {
    entries.clear();
    for (auto& entry : list) {
        entries.insert(entry.id, entry);
    }
    return *this;
}

template <typename TEnum>
Settings<TEnum>& Settings<TEnum>::operator=(const Settings& other) {
    entries = other.entries.clone();
    return *this;
}

template <typename TEnum>
Settings<TEnum>& Settings<TEnum>::operator=(Settings&& other) {
    entries = std::move(other.entries);
    return *this;
}

template <typename TEnum>
bool Settings<TEnum>::setValueByType(Entry& entry, const Value& defaultValue, const std::string& str) {
    const Size typeIdx = defaultValue.getTypeIdx();
    std::stringstream ss(str.c_str());
    switch (typeIdx) {
    case BOOL: {
        std::string value;
        ss >> value;
        if (ss.fail()) {
            return false;
        } else if (value == "true") {
            entry.value = true;
            return true;
        } else if (value == "false") {
            entry.value = false;
            return true;
        } else {
            return false;
        }
    }
    case INT:
        int i;
        ss >> i;
        if (ss.fail()) {
            return false;
        } else {
            entry.value = i;
            return true;
        }
    case FLOAT:
        Float f;
        ss >> f;
        if (ss.fail()) {
            return false;
        } else {
            entry.value = f;
            return true;
        }
    case INTERVAL: {
        std::string s1, s2;
        ss >> s1 >> s2;
        if (ss.fail()) {
            return false;
        }
        Float lower, upper;
        if (s1 == "-infinity") {
            lower = -INFTY;
        } else {
            ss.clear();
            ss.str(s1);
            Float value;
            ss >> value;
            lower = value;
        }
        if (s2 == "infinity") {
            upper = INFTY;
        } else {
            ss.clear();
            ss.str(s2);
            Float value;
            ss >> value;
            upper = value;
        }
        if (ss.fail()) {
            return false;
        } else {
            entry.value = Interval(lower, upper);
            return true;
        }
    }
    case STRING: {
        // trim leading and trailing spaces
        const std::string trimmed = trim(str);
        entry.value = trimmed;
        return true;
    }
    case VECTOR:
        Float v1, v2, v3;
        ss >> v1 >> v2 >> v3;
        if (ss.fail()) {
            return false;
        } else {
            entry.value = Vector(v1, v2, v3);
            return true;
        }
    case SYMMETRIC_TENSOR:
        Float sxx, syy, szz, sxy, sxz, syz;
        ss >> sxx >> syy >> szz >> sxy >> sxz >> syz;
        if (ss.fail()) {
            return false;
        } else {
            entry.value = SymmetricTensor(Vector(sxx, syy, szz), Vector(sxy, sxz, syz));
            return true;
        }
    case TRACELESS_TENSOR:
        Float txx, tyy, txy, txz, tyz;
        ss >> txx >> tyy >> txy >> txz >> tyz;
        if (ss.fail()) {
            return false;
        } else {
            entry.value = TracelessTensor(txx, tyy, txy, txz, tyz);
            return true;
        }
    case ENUM: {
        const std::size_t id = defaultValue.template get<EnumWrapper>().typeHash;
        std::string textValue;
        int flags = 0;
        while (true) {
            ss >> textValue;
            if (ss.fail()) {
                return false;
            }
            if (textValue == "0") {
                // special value representing empty flags;
                // we need to check that this is the only thing on the line
                ss >> textValue;
                if (!ss && flags == 0) {
                    flags = 0;
                    break;
                } else {
                    return false;
                }
            }
            Optional<int> value = EnumMap::fromString(textValue, id);
            if (!value) {
                return false;
            } else {
                flags |= value.value();
            }
            ss >> textValue;
            if (!ss || textValue != "|") {
                break;
            }
        }
        entry.value = EnumWrapper(flags, id);
        return true;
    }
    default:
        NOT_IMPLEMENTED;
    }
}

template <typename TEnum>
Outcome Settings<TEnum>::loadFromFile(const Path& path) {
    std::ifstream ifs(path.native());
    if (!ifs) {
        return "File " + path.native() + " cannot be opened for reading.";
    }
    std::string line;
    const Settings& descriptors = getDefaults();
    while (std::getline(ifs, line, '\n')) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const Size idx = Size(line.find("=", 0));
        if (idx == std::string::npos) {
            return "Invalid format of the file, didn't find separating '='";
        }
        std::string key = line.substr(0, idx);
        std::string value = line.substr(idx + 1);
        // throw away spaces from key
        std::string trimmedKey = trim(key);

        // find the key in decriptor settings
        bool found = false;
        for (auto& e : descriptors.entries) {
            if (e.value.name == trimmedKey) {
                entries.insert(e.value.id, e.value);
                if (!setValueByType(entries[e.value.id], e.value.value, value)) {
                    return "Invalid value of key " + trimmedKey + ": " + value;
                }
                found = true;
                break;
            }
        }
        if (!found) {
            return "Key " + trimmedKey + " was not find in settings";
        }
    }
    ifs.close();
    return SUCCESS;
}

template <typename TEnum>
Outcome Settings<TEnum>::saveToFile(const Path& path) const {
    const Outcome dirCreated = FileSystem::createDirectory(path.parentPath());
    if (!dirCreated) {
        return "Cannot save settings: " + dirCreated.error();
    }

    const Settings& descriptors = getDefaults();
    try {
        std::ofstream ofs(path.native());
        for (auto& e : entries) {
            const Entry& entry = e.value;
            const Entry& descriptor = descriptors.entries[e.key];
            if (!descriptor.desc.empty()) {
                std::string desc = "# " + descriptor.desc;
                desc = setLineBreak(desc, 120);
                desc = replaceAll(desc, "\n", "\n# ");
                ofs << desc << std::endl;
            }

            ofs << std::setw(30) << std::left << descriptor.name << " = ";
            switch (entry.value.getTypeIdx()) {
            case BOOL:
                ofs << (entry.value.template get<bool>() ? "true" : "false");
                break;
            case INT:
                ofs << entry.value.template get<int>();
                break;
            case FLOAT:
                ofs << entry.value.template get<Float>();
                break;
            case INTERVAL:
                ofs << entry.value.template get<Interval>();
                break;
            case STRING:
                ofs << entry.value.template get<std::string>();
                break;
            case VECTOR:
                ofs << entry.value.template get<Vector>();
                break;
            case SYMMETRIC_TENSOR:
                ofs << entry.value.template get<SymmetricTensor>();
                break;
            case TRACELESS_TENSOR:
                ofs << entry.value.template get<TracelessTensor>();
                break;
            case ENUM: {
                EnumWrapper e = entry.value.template get<EnumWrapper>();
                ofs << EnumMap::toString(e.value, e.typeHash);
                break;
            }
            default:
                NOT_IMPLEMENTED;
            }
            ofs << std::endl;
        }
        ofs.close();
        return SUCCESS;
    } catch (std::exception& e) {
        return std::string("Cannot save settings: ") + e.what();
    }
}

template <typename TEnum>
bool Settings<TEnum>::tryLoadFileOrSaveCurrent(const Path& path, const Settings& overrides) {
    if (FileSystem::pathExists(path)) {
        // load from file and apply the overrides
        const Outcome result = this->loadFromFile(path);
        if (!result) {
            throw IoError(result.error());
        }
        this->addEntries(overrides);
        return true;
    } else {
        // apply overrides and then save
        this->addEntries(overrides);
        this->saveToFile(path);
        return false;
    }
}

template <typename TEnum>
SettingsIterator<TEnum> Settings<TEnum>::begin() const {
    return SettingsIterator<TEnum>(entries.begin());
}

template <typename TEnum>
SettingsIterator<TEnum> Settings<TEnum>::end() const {
    return SettingsIterator<TEnum>(entries.end());
}

template <typename TEnum>
Size Settings<TEnum>::size() const {
    return entries.size();
}

template <typename TEnum>
const Settings<TEnum>& Settings<TEnum>::getDefaults() {
    ASSERT(instance != nullptr);
    return *instance;
}

template <typename TEnum>
SettingsIterator<TEnum>::SettingsIterator(const ActIterator& iter)
    : iter(iter) {}

template <typename TEnum>
typename SettingsIterator<TEnum>::IteratorValue SettingsIterator<TEnum>::operator*() const {
    return { iter->key, iter->value.value };
}

template <typename TEnum>
SettingsIterator<TEnum>& SettingsIterator<TEnum>::operator++() {
    ++iter;
    return *this;
}

template <typename TEnum>
bool SettingsIterator<TEnum>::operator==(const SettingsIterator& other) const {
    return iter == other.iter;
}

template <typename TEnum>
bool SettingsIterator<TEnum>::operator!=(const SettingsIterator& other) const {
    return iter != other.iter;
}

NAMESPACE_SPH_END
