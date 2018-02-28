#include "objects/utility/StringUtils.h"

NAMESPACE_SPH_BEGIN

template <typename T>
Optional<T> fromString(const std::string& s) {
    std::stringstream ss(s);
    T value;
    ss >> value;
    if (ss.fail()) {
        return NOTHING;
    } else {
        return value;
    }
}

template <>
Optional<std::string> fromString(const std::string& s) {
    return s;
}

template Optional<int> fromString(const std::string& s);
template Optional<Size> fromString(const std::string& s);
template Optional<float> fromString(const std::string& s);


std::string trim(const std::string& s) {
    Size i1 = 0;
    for (; i1 < s.size(); ++i1) {
        if (s[i1] != ' ') {
            break;
        }
    }
    Size i2 = s.size();
    for (; i2 > 0; --i2) {
        if (s[i2 - 1] != ' ') {
            break;
        }
    }
    std::string trimmed;
    for (Size i = i1; i < i2; ++i) {
        trimmed.push_back(s[i]);
    }
    return trimmed;
}

std::string lowercase(const std::string& s) {
    std::string l = s;
    for (char& c : l) {
        if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
    }
    return l;
}

std::string replaceFirst(const std::string& source, const std::string& old, const std::string& s) {
    const auto n = source.find(old);
    if (n == std::string::npos) {
        return source;
    }
    std::string replaced = source;
    replaced.replace(n, old.size(), s);
    return replaced;
}

std::string replaceAll(const std::string& source, const std::string& old, const std::string& s) {
    std::string current = source;
    while (true) {
        std::string replaced = replaceFirst(current, old, s);
        if (replaced == current) {
            return current;
        } else {
            current = replaced;
        }
    }
}

Array<std::string> split(const std::string& s, const char delimiter) {
    Array<std::string> parts;
    std::size_t n1 = -1; // yes, -1, unsigned int overflow is well defined
    std::size_t n2;
    while ((n2 = s.find(delimiter, n1 + 1)) != std::string::npos) {
        parts.push(s.substr(n1 + 1, n2 - n1 - 1));
        n1 = n2;
    }
    // add the last part
    parts.push(s.substr(n1 + 1));
    return parts;
}


NAMESPACE_SPH_END
