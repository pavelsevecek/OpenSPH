#include "objects/containers/String.h"
#include <iomanip>

NAMESPACE_SPH_BEGIN

constexpr Size String::npos;

Size String::find(const String& s, const Size pos) const {
    ASSERT(pos < this->size() && !s.empty(), pos, this->size());
    if (s.size() > this->size()) {
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

Size String::findAny(ArrayView<String> ss, const Size pos) const {
    Size n = npos;
    for (String& s : ss) {
        n = min(n, this->find(s, pos));
    }
    return n;
}

Size String::findLast(const String& s) const {
    ASSERT(!s.empty());
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

void String::replace(const Size pos, const Size n, const String& s) {
    ASSERT(pos + n <= this->size());
    Array<char> replaced;
    replaced.reserve(data.size() + s.size() - n);
    for (Size i = 0; i < pos; ++i) {
        replaced.push(data[i]);
    }
    for (Size i = 0; i < s.size(); ++i) {
        replaced.push(s[i]);
    }
    for (Size i = pos + n; i < data.size(); ++i) {
        replaced.push(data[i]);
    }
    *this = String(std::move(replaced));
}

void String::replace(const String& old, const String& s) {
    const Size n = this->find(old);
    if (n == npos) {
        return;
    }
    this->replace(n, old.size(), s);
}

String String::substr(const Size pos, const Size n) const {
    ASSERT(pos < this->size());
    Array<char> ss;
    const Size m = min(n, this->size() - pos);
    ss.reserve(m + 1);
    for (Size i = pos; i < pos + m; ++i) {
        ss.push(data[i]);
    }
    ss.push('\0');
    return ss;
}

String String::trim() const {
    Size i1 = 0;
    for (; i1 < data.size(); ++i1) {
        if (data[i1] != ' ') {
            break;
        }
    }
    Size i2 = data.size() - 1;
    for (; i2 > 0; --i2) {
        if (data[i2 - 1] != ' ') {
            break;
        }
    }
    Array<char> trimmed;
    for (Size i = i1; i < i2; ++i) {
        trimmed.push(data[i]);
    }
    trimmed.push('\0');
    return trimmed;
}

String String::lower() const {
    String s = *this;
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
    }
    return s;
}


String getFormattedTime(const String& format) {
    std::time_t t = std::time(nullptr);
    char buffer[256];
    auto retval = strftime(buffer, sizeof(buffer), format.cStr(), std::localtime(&t));
    String s(buffer);
    ASSERT(retval == s.size());
    return s;
}


NAMESPACE_SPH_END
