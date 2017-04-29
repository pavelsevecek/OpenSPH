#include "objects/containers/StringUtils.h"

NAMESPACE_SPH_BEGIN


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

std::string replace(const std::string& source, const std::string& old, const std::string& s) {
    const auto n = source.find(old);
    if (n == std::string::npos) {
        return source;
    }
    std::string replaced = source;
    replaced.replace(n, old.size(), s);
    return replaced;
}


NAMESPACE_SPH_END
