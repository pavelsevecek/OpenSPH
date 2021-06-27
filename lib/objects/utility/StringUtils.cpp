#include "objects/utility/StringUtils.h"

NAMESPACE_SPH_BEGIN

template <>
Optional<std::string> fromString(const std::string& s) {
    return s;
}

template <>
Optional<int> fromString(const std::string& s) {
    try {
        std::size_t idx;
        const int result = std::stoi(s, &idx);
        if (idx == s.size()) {
            return result;
        } else {
            return NOTHING;
        }
    } catch (std::exception&) {
        return NOTHING;
    }
}

template <>
Optional<Size> fromString(const std::string& s) {
    try {
        std::size_t idx;
        const Size result = std::stoul(s, &idx);
        if (idx == s.size()) {
            return result;
        } else {
            return NOTHING;
        }
    } catch (std::exception&) {
        return NOTHING;
    }
}

template <>
Optional<float> fromString(const std::string& s) {
    try {
        std::size_t idx;
        const float result = std::stof(s, &idx);
        if (idx == s.size()) {
            return result;
        } else {
            return NOTHING;
        }
    } catch (std::exception&) {
        return NOTHING;
    }
}

template <>
Optional<double> fromString(const std::string& s) {
    try {
        std::size_t idx;
        const double result = std::stod(s, &idx);
        if (idx == s.size()) {
            return result;
        } else {
            return NOTHING;
        }
    } catch (std::exception&) {
        return NOTHING;
    }
}

bool startsWith(const std::string& s, const std::string& start) {
    return s.size() >= start.size() && s.substr(0, start.size()) == start;
}

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
    const std::size_t n = source.find(old);
    if (n == std::string::npos) {
        return source;
    }
    std::string replaced = source;
    replaced.replace(n, old.size(), s);
    return replaced;
}

std::string replaceAll(const std::string& source, const std::string& old, const std::string& s) {
    std::string current = source;
    std::size_t pos = 0;
    while (true) {
        const std::size_t n = current.find(old, pos);
        if (n == std::string::npos) {
            return current;
        }
        current.replace(n, old.size(), s);
        pos = n + s.size();
    }
}

std::string setLineBreak(const std::string& s, const Size lineWidth) {
    const std::string emptyChars = " \t\r";
    const std::string canBreakChars = ".,;\n" + emptyChars;
    std::string result = s;
    std::size_t lastLineBreak = 0;
    std::size_t lastSpaceNum = 0;
    bool commaFound = false;

    for (std::size_t n = 0; n < result.size();) {
        // find the next possible break
        std::size_t pos = result.find_first_of(canBreakChars, n);
        if (pos == std::string::npos) {
            pos = result.size();
        }
        if (result[pos] == '\n') {
            // there already is a line break, reset the counter and continue
            n = pos + 1;
            lastLineBreak = n;
            commaFound = false;
            lastSpaceNum = 0;
            continue;
        }
        if (pos - lastLineBreak <= lineWidth) {
            // no need to break
            n = pos + 1;
            continue;
        } else {
            // remove all empty chars before the break
            --n;
            while (n < result.size() && emptyChars.find(result[n]) != std::string::npos) {
                result.erase(n, 1);
                --n;
            }
            ++n;
            // insert a line break here
            result.insert(n++, "\n");

            if (commaFound && lastSpaceNum > 0) {
                result.insert(n, std::string(lastSpaceNum, ' '));
                n += lastSpaceNum;
            }
            // indent if there is a pattern ' - %s: ' on the previous line
            const std::size_t comma = result.find("- ", lastLineBreak);
            if (comma < n) {
                const std::size_t colon = result.find(": ", comma);
                if (colon < n) {
                    std::size_t spaceNum = colon + 2 - lastLineBreak;
                    result.insert(n, std::string(spaceNum, ' '));
                    n += spaceNum;
                    lastSpaceNum = spaceNum;
                    commaFound = true;
                }
            }

            lastLineBreak = n;

            // remove all following empty chars
            while (n < result.size() && emptyChars.find(result[n]) != std::string::npos) {
                result.erase(n, 1);
            }
        }
    }
    return result;
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

Pair<std::string> splitByFirst(const std::string& s, const char delimiter) {
    const std::size_t n = s.find(delimiter);
    if (n == std::string::npos) {
        return {};
    } else {
        Pair<std::string> parts;
        parts[0] = s.substr(0, n);
        parts[1] = s.substr(n + 1);
        return parts;
    }
}

static Array<std::string> capitalizationBlacklist{ "and", "or", "of", "for", "to", "et", "al" };

static bool shouldCapitalize(const std::string& s) {
    for (const std::string& b : capitalizationBlacklist) {
        if (s.size() < b.size()) {
            continue;
        }
        if (s.substr(0, b.size()) == b && (s.size() == b.size() || s[b.size()] == ' ')) {
            return false;
        }
    }
    return true;
}

std::string capitalize(const std::string& input) {
    std::string result = input;
    for (Size i = 0; i < result.size(); ++i) {
        if (i == 0 || (result[i - 1] == ' ' && shouldCapitalize(result.substr(i)))) {
            result[i] = toupper(result[i]);
        }
    }
    return result;
}

UniqueNameManager::UniqueNameManager(ArrayView<const std::string> initial) {
    for (const std::string& name : initial) {
        names.insert(name);
    }
}

std::string UniqueNameManager::getName(const std::string& name) {
    std::string tested = name;

    for (Size postfix = 1; postfix < 999; ++postfix) {
        if (names.find(tested) == names.end()) {
            names.insert(tested);
            return tested;
        } else {
            /// \todo increase the number if there is already a (x) at the end
            tested = name + " (" + std::to_string(postfix) + ")";
        }
    }
    return name; /// \todo what to return?
}

NAMESPACE_SPH_END
