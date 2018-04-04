#include "io/FileSystem.h"
#include "objects/utility/StringUtils.h"
#include "objects/wrappers/Outcome.h"
#include <fstream>
#include <regex>

NAMESPACE_SPH_BEGIN

template <typename TEnum>
void Settings<TEnum>::saveToFile(const Path& path) const {
    FileSystem::createDirectory(path.parentPath());
    std::ofstream ofs(path.native());
    for (auto& e : entries) {
        const Entry& entry = e.second;
        ofs << std::setw(30) << std::left << entry.name << " = ";
        switch (entry.value.getTypeIdx()) {
        case BOOL:
            ofs << entry.value.template get<bool>();
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
        default:
            NOT_IMPLEMENTED;
        }
        ofs << std::endl;
    }
    ofs.close();
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
        const Size idx = line.find("=", 0);
        if (idx == std::string::npos) {
            return "Invalid format of the file, didn't find separating '='";
        }
        std::string key = line.substr(0, idx);
        std::string value = line.substr(idx + 1);
        // throw away spaces from key
        std::string trimmedKey = trim(key);

        // find the key in decriptor settings
        bool found = false;
        for (auto&& e : descriptors.entries) {
            if (e.second.name == trimmedKey) {
                if (!setValueByType(this->entries[e.second.id], e.second.value.getTypeIdx(), value)) {
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
bool Settings<TEnum>::setValueByType(Entry& entry, const Size typeIdx, const std::string& str) {
    std::stringstream ss(str.c_str());
    switch (typeIdx) {
    case BOOL: {
        bool b;
        ss >> b;
        if (ss.fail()) {
            return false;
        } else {
            entry.value = b;
            return true;
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
    default:
        NOT_IMPLEMENTED;
    }
}


template <typename TEnum>
SettingsIterator<TEnum>::SettingsIterator(const Iterator& iter)
    : iter(iter) {}

template <typename TEnum>
typename SettingsIterator<TEnum>::IteratorValue SettingsIterator<TEnum>::operator*() const {
    return { iter->first, iter->second.value };
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


// Explicit instantiation
/*template class Settings<BodySettingsId>;
template class SettingsIterator<BodySettingsId>;

template class Settings<RunSettingsId>;
template class SettingsIterator<RunSettingsId>;*/

NAMESPACE_SPH_END
