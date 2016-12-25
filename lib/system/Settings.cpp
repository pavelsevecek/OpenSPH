#include "system/Settings.h"
#include <fstream>
#include <iostream>
#include <regex>

NAMESPACE_SPH_BEGIN

template <typename TEnum>
void Settings<TEnum>::saveToFile(const std::string& path) const {
    std::ofstream ofs(path);
    for (auto& e : entries) {
        const Entry& entry = e.second;
        ofs << std::setw(30) << std::left << entry.name << " = ";
        switch (entry.value.getTypeIdx()) {
        case BOOL:
            ofs << (bool)entry.value;
            break;
        case INT:
            ofs << (int)entry.value;
            break;
        case FLOAT:
            ofs << (Float)entry.value;
            break;
        case RANGE:
            ofs << entry.value.get<Range>().get();
            break;
        case STRING:
            ofs << entry.value.get<std::string>().get();
            break;
        case VECTOR:
            ofs << entry.value.get<Vector>().get();
            break;
        case TENSOR:
            ofs << entry.value.get<Tensor>().get();
            break;
        case TRACELESS_TENSOR:
            ofs << entry.value.get<TracelessTensor>().get();
            break;
        default:
            NOT_IMPLEMENTED;
        }
        ofs << std::endl;
    }
    ofs.close();
}

template <typename TEnum>
bool Settings<TEnum>::loadFromFile(const std::string& path, const Settings& descriptors) {
    std::ifstream ifs(path);
    std::string line;
    while (std::getline(ifs, line, '\n')) {
        std::string::size_type idx = line.find("=");
        if (idx == std::string::npos) {
            // didn't find '=', invalid format of the file
            return false;
        }
        std::string key = line.substr(0, idx);
        std::string value = line.substr(idx + 1);
        // throw away spaces from key
        std::string trimmedKey;
        for (const char c : key) {
            if (c != ' ') {
                trimmedKey.push_back(c);
            }
        }
        // find the key in decriptor settings
        for (auto&& e : descriptors.entries) {
            if (e.second.name == trimmedKey) {
                if (!setValueByType(this->entries[e.second.id], e.second.value.getTypeIdx(), value)) {
                    std::cout << "failed loading " << trimmedKey << std::endl;
                    return false;
                }
            }
        }
    }
    ifs.close();
    return true;
}

template <typename TEnum>
bool Settings<TEnum>::setValueByType(Entry& entry, const int typeIdx, const std::string& str) {
    std::stringstream ss(str);
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
    case RANGE: {
        std::string s1, s2;
        ss >> s1 >> s2;
        if (ss.fail()) {
            return false;
        }
        Optional<Float> lower, upper;
        if (s1 == "-infinity") {
            lower = NOTHING;
        } else {
            ss.clear();
            ss.str(s1);
            Float value;
            ss >> value;
            lower = value;
        }
        if (s2 == "infinity") {
            upper = NOTHING;
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
            entry.value = Range(lower, upper);
            return true;
        }
    }
    case STRING: {
        // trim leading and trailing spaces
        const std::string trimmed = std::regex_replace(str, std::regex("^ +| +$|( ) +"), "$1");
        ASSERT(!trimmed.empty() && "Variant cannot handle empty strings");
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
    case TENSOR:
        Float sxx, syy, szz, sxy, sxz, syz;
        ss >> sxx >> syy >> szz >> sxy >> sxz >> syz;
        if (ss.fail()) {
            return false;
        } else {
            entry.value = Tensor(Vector(sxx, syy, szz), Vector(sxy, sxz, syz));
            return true;
        }
    case TRACELESS_TENSOR:
        Float txx, tyy, txy, txz, tyz;
        ss >> txx >> tyy >> txy >> txz >> tyz;
        if (ss.fail()) {
            return false;
        } else {
            entry.value = TracelessTensor(txx, tyy, txy, txz, tyz);
        }
    default:
        NOT_IMPLEMENTED;
    }
}


// Explicit instantiation
template class Settings<BodySettingsIds>;
template class Settings<GlobalSettingsIds>;


NAMESPACE_SPH_END
