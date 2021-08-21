#include "objects/containers/String.h"
#include "objects/wrappers/Optional.h"
#include "thread/CheckFunction.h"
#include <codecvt>
#include <cwchar>
#include <cwctype>
#include <iomanip>
#include <locale>

NAMESPACE_SPH_BEGIN

Size String::npos = NumericLimits<Size>::max();

String::String(const wchar_t* s) {
    data.pop();
    const Size length = Size(std::wcslen(s));
    data.reserve(length + 1);
    for (Size i = 0; i < length; ++i) {
        data.push(s[i]);
    }
    data.push(L'\0');
}

String String::fromAscii(const char* s) {
    Array<wchar_t> data;
    const Size length = Size(std::strlen(s));
    data.reserve(length + 1);
    for (Size i = 0; i < length; ++i) {
        data.push(s[i]);
    }
    data.push(L'\0');
    return data;
}

String String::fromUtf8(const char* s) {
    CHECK_FUNCTION(CheckFunction::NO_THROW);
    std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convert;
    try {
        std::u16string s16 = convert.from_bytes(s);
        return String::fromWstring(std::wstring(s16.begin(), s16.end()));
    } catch (const std::range_error& e) {
        SPH_ASSERT(false, e.what());
        return String::fromAscii(s);
    }
}

CharString String::toAscii() const {
    Array<char> chars;
    chars.resize(data.size());
    for (Size i = 0; i < data.size(); ++i) {
        if (data[i] <= 127) {
            chars[i] = char(data[i]);
        } else {
            SPH_ASSERT(false, "Is there a valid usecase of this??");
            chars[i] = '_';
        }
    }
    return CharString(chars);
}

bool String::isAscii() const {
    for (Size i = 0; i < data.size(); ++i) {
        if (data[i] > 127) {
            return false;
        }
    }
    return true;
}

CharString String::toUtf8() const {
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> convert;
    std::string str = convert.to_bytes(&data[0]);
    return ArrayView<const char>(str.data(), str.size());
}

std::wstring String::toWstring() const {
    return std::wstring(this->toUnicode());
}

Size String::find(const String& s, const Size pos) const {
    SPH_ASSERT(pos <= this->size());
    if (s.size() + pos > this->size()) {
        return npos;
    }
    for (Size i = pos; i <= this->size() - s.size(); ++i) {
        if (data[i] == s[0]) {
            bool matching = true;
            for (Size j = 1; j < s.size(); ++j) {
                if (data[i + j] != s[j]) {
                    matching = false;
                    break;
                }
            }
            if (matching) {
                return i;
            }
        }
    }
    return npos;
}

Size String::find(const wchar_t c, const Size pos) const {
    SPH_ASSERT(pos <= this->size());
    for (Size i = pos; i < this->size(); ++i) {
        if (data[i] == c) {
            return i;
        }
    }
    return npos;
}

Size String::findAny(ArrayView<const String> ss, const Size pos) const {
    Size n = npos;
    for (const String& s : ss) {
        n = min(n, this->find(s, pos));
    }
    return n;
}

Size String::findAny(ArrayView<const wchar_t> cs, const Size pos) const {
    Size n = npos;
    for (wchar_t c : cs) {
        n = min(n, this->find(c, pos));
    }
    return n;
}

Size String::findLast(const String& s) const {
    SPH_ASSERT(!s.empty());
    if (s.size() > this->size()) {
        return npos;
    }
    for (Size i = this->size() - s.size() + 1; i > 0; --i) {
        if (data[i - 1] == s[0]) {
            bool matching = true;
            for (Size j = 1; j < s.size(); ++j) {
                if (data[i + j - 1] != s[j]) {
                    matching = false;
                    break;
                }
            }
            if (matching) {
                return i - 1;
            }
        }
    }
    return npos;
}

Size String::findLast(const wchar_t c) const {
    for (Size i = this->size(); i > 0; --i) {
        if (data[i - 1] == c) {
            return i - 1;
        }
    }
    return npos;
}

void String::replace(const Size pos, const Size n, const String& s) {
    Size n1 = (n == npos) ? this->size() - pos : n;
    SPH_ASSERT(pos + n1 <= this->size());
    Array<wchar_t> replaced;
    replaced.reserve(data.size() + s.size() - n1);
    for (Size i = 0; i < pos; ++i) {
        replaced.push(data[i]);
    }
    for (Size i = 0; i < s.size(); ++i) {
        replaced.push(s[i]);
    }
    for (Size i = pos + n1; i < data.size(); ++i) {
        replaced.push(data[i]);
    }
    *this = String(std::move(replaced));
}

bool String::replaceFirst(const String& old, const String& s) {
    const Size n = this->find(old);
    if (n == npos) {
        return false;
    }
    this->replace(n, old.size(), s);
    return true;
}

Size String::replaceAll(const String& old, const String& s) {
    Size count = 0;
    String current = *this;
    Size pos = 0;
    while (true) {
        const Size n = current.find(old, pos);
        if (n == String::npos) {
            *this = current;
            return count;
        }
        current.replace(n, old.size(), s);
        ++count;
        pos = n + s.size();
    }
}

void String::insert(const Size pos, const String& s) {
    SPH_ASSERT(pos <= this->size());
    data.insert(pos, s.begin(), s.end());
}

void String::erase(const Size pos, const Size n) {
    SPH_ASSERT(pos + n <= this->size());
    data.remove(data.begin() + pos, data.begin() + pos + n);
}

void String::clear() {
    data.clear();
    data.push(L'\0');
}

String String::substr(const Size pos, const Size n) const {
    SPH_ASSERT(pos <= this->size());
    Array<wchar_t> ss;
    const Size m = min(n, this->size() - pos);
    ss.reserve(m + 1);
    for (Size i = pos; i < pos + m; ++i) {
        ss.push(data[i]);
    }
    ss.push(L'\0');
    return ss;
}

static bool shouldTrim(const wchar_t c, const Flags<String::TrimFlag> flags) {
    return (flags.has(String::TrimFlag::SPACE) && c == L' ') ||
           (flags.has(String::TrimFlag::END_LINE) && c == L'\n') ||
           (flags.has(String::TrimFlag::TAB) && c == L'\t');
}

String String::trim(const Flags<TrimFlag> flags) const {
    Size i1 = 0;
    for (; i1 < data.size(); ++i1) {
        if (!shouldTrim(data[i1], flags)) {
            break;
        }
    }
    Size i2 = data.size() - 1;
    for (; i2 > 0; --i2) {
        if (!shouldTrim(data[i2 - 1], flags)) {
            break;
        }
    }
    Array<wchar_t> trimmed;
    for (Size i = i1; i < i2; ++i) {
        trimmed.push(data[i]);
    }
    trimmed.push(L'\0');
    return trimmed;
}

String String::toLowercase() const {
    String s = *this;
    for (wchar_t& c : s) {
        c = std::towlower(c);
    }
    return s;
}

template <>
Optional<String> fromString(const String& s) {
    return s;
}

template <>
Optional<bool> fromString(const String& s) {
    try {
        std::size_t idx;
        /// \todo could be a StringView
        String trimmed = s.trim(String::TrimFlag::SPACE | String::TrimFlag::END_LINE);
        const bool value = bool(std::stoi(trimmed.toWstring(), &idx));
        if (idx == trimmed.size()) {
            return value;
        } else {
            return NOTHING;
        }
    } catch (const std::exception&) {
        return NOTHING;
    }
}

template <>
Optional<int> fromString(const String& s) {
    try {
        std::size_t idx;
        String trimmed = s.trim(String::TrimFlag::SPACE | String::TrimFlag::END_LINE);
        const int value = std::stoi(trimmed.toWstring(), &idx);
        if (idx == trimmed.size()) {
            return value;
        } else {
            return NOTHING;
        }
    } catch (const std::exception&) {
        return NOTHING;
    }
}

template <>
Optional<Size> fromString(const String& s) {
    try {
        std::size_t idx;
        String trimmed = s.trim(String::TrimFlag::SPACE | String::TrimFlag::END_LINE);
        const Size value = std::stoul(trimmed.toWstring(), &idx);
        if (idx == trimmed.size()) {
            return value;
        } else {
            return NOTHING;
        }
    } catch (const std::exception&) {
        return NOTHING;
    }
}

template <>
Optional<float> fromString(const String& s) {
    try {
        std::size_t idx;
        String trimmed = s.trim(String::TrimFlag::SPACE | String::TrimFlag::END_LINE);
        const float value = std::stof(trimmed.toWstring(), &idx);
        if (idx == trimmed.size()) {
            return value;
        } else {
            return NOTHING;
        }
    } catch (const std::exception&) {
        return NOTHING;
    }
}

template <>
Optional<double> fromString(const String& s) {
    try {
        std::size_t idx;
        String trimmed = s.trim(String::TrimFlag::SPACE | String::TrimFlag::END_LINE);
        const double value = std::stod(trimmed.toWstring(), &idx);
        if (idx == trimmed.size()) {
            return value;
        } else {
            return NOTHING;
        }
    } catch (const std::exception&) {
        return NOTHING;
    }
}

String exceptionMessage(const std::exception& e) {
    CHECK_FUNCTION(CheckFunction::NO_THROW);
    return String::fromUtf8(e.what());
}

String setLineBreak(const String& s, const Size lineWidth) {
    const static String emptyChars = " \t\r";
    const static String canBreakChars = ".,;!?\n)]" + emptyChars;
    String result = s;
    Size lastLineBreak = 0;
    Size lastSpaceNum = 0;
    bool commaFound = false;

    for (Size n = 0; n < result.size();) {
        // find the next possible break
        Size pos = result.findAny(canBreakChars.view(), n);
        if (pos == String::npos) {
            pos = result.size();
        }
        if (pos < result.size() && result[pos] == '\n') {
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
            while (n < result.size() && emptyChars.find(result[n]) != String::npos) {
                result.erase(n, 1);
                --n;
            }
            ++n;

            if (n > 0) {
                // insert a line break here
                result.insert(n, "\n");
            }

            n++;

            if (commaFound && lastSpaceNum > 0) {
                result.insert(n, String::fromChar(' ', lastSpaceNum));
                n += lastSpaceNum;
            }
            // indent if there is a pattern ' - %s: ' on the previous line
            const std::size_t comma = result.find("- ", lastLineBreak);
            if (comma < n) {
                const std::size_t colon = result.find(": ", comma);
                if (colon < n) {
                    Size spaceNum = colon + 2 - lastLineBreak;
                    result.insert(n, String::fromChar(' ', spaceNum));
                    n += spaceNum;
                    lastSpaceNum = spaceNum;
                    commaFound = true;
                }
            }

            lastLineBreak = n;

            // remove all following empty chars
            while (n < result.size() && emptyChars.find(result[n]) != String::npos) {
                result.erase(n, 1);
            }

            n = result.findAny(canBreakChars.view(), n);
        }
    }
    return result;
}

Array<String> split(const String& s, const wchar_t delimiter) {
    Array<String> parts;
    Size n1 = Size(-1); // yes, -1, unsigned int overflow is well defined
    Size n2;
    while ((n2 = s.find(delimiter, n1 + 1)) != String::npos) {
        parts.push(s.substr(n1 + 1, n2 - n1 - 1));
        n1 = n2;
    }
    // add the last part
    parts.push(s.substr(n1 + 1));
    return parts;
}

Pair<String> splitByFirst(const String& s, const wchar_t delimiter) {
    const Size n = s.find(delimiter);
    if (n == String::npos) {
        return {};
    } else {
        Pair<String> parts;
        parts[0] = s.substr(0, n);
        parts[1] = s.substr(n + 1);
        return parts;
    }
}


static Array<String> capitalizationBlacklist{ "and", "or", "of", "for", "to", "et", "al" };

static bool shouldCapitalize(const String& s) {
    for (const String& b : capitalizationBlacklist) {
        if (s.size() < b.size()) {
            continue;
        }
        if (s.substr(0, b.size()) == b && (s.size() == b.size() || s[b.size()] == ' ')) {
            return false;
        }
    }
    return true;
}

String capitalize(const String& input) {
    String result = input;
    for (Size i = 0; i < result.size(); ++i) {
        if (i == 0 || (result[i - 1] == ' ' && shouldCapitalize(result.substr(i)))) {
            result[i] = std::towupper(result[i]);
        }
    }
    return result;
}

NAMESPACE_SPH_END
