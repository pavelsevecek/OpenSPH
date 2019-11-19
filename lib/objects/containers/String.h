#pragma once

/// \file String.h
/// \brief Object representing a sequence of characters
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "objects/containers/Array.h"
#include <string.h>

NAMESPACE_SPH_BEGIN

class String {
private:
    Array<char> data = { '\0' };

public:
    String() = default;

    String(const String& other)
        : data(other.data.clone()) {}

    String(String&& other) {
        // we need to put '\0' before swapping to leave other in consistent state
        data = std::move(other.data);
    }

    String(Array<char>&& buffer)
        : data(std::move(buffer)) {
        ASSERT(sanityCheck());
    }

    String(const char* s) {
        data.pop(); // pop terminating zero
        const Size length = strlen(s);
        data.reserve(length + 1);
        for (Size i = 0; i < length; ++i) {
            data.push(s[i]);
        }
        data.push('\0');
    }

    String& operator=(const String& other) {
        data = copyable(other.data);
        return *this;
    }

    String& operator=(String&& other) {
        data = std::move(other.data);
        return *this;
    }

    String& operator+=(const String& other) {
        data.pop();               // pop terminating zero
        data.pushAll(other.data); // push all characters including terminating zero
        ASSERT(sanityCheck());
        return *this;
    }

    friend String operator+(const String& s1, const String& s2) {
        String s = s1;
        s += s2;
        return s;
    }

    String& operator+=(const char c) {
        data.pop();
        data.push(c);
        data.push('\0');
        return *this;
    }

    bool operator==(const String& other) const {
        return data == other.data;
    }

    bool operator!=(const String& other) const {
        return data != other.data;
    }

    const char* cStr() const {
        return &data[0];
    }

    INLINE char operator[](const Size idx) const {
        ASSERT(idx < this->size());
        return data[idx];
    }

    INLINE Size size() const {
        return data.size() - 1;
    }

    INLINE bool empty() const {
        return data.size() == 1;
    }

    /// \section Iterators

    Iterator<char> begin() {
        return data.begin();
    }

    Iterator<const char> begin() const {
        return data.begin();
    }

    Iterator<char> end() {
        return data.end() - 1;
    }

    Iterator<const char> end() const {
        return data.end() - 1;
    }

    friend std::ostream& operator<<(std::ostream& stream, const String& str) {
        stream << str.cStr();
        return stream;
    }

    /// \addtogroup utility Utility functions

    static Size npos;

    Size find(const String& s, const Size pos = 0) const;

    Size findAny(ArrayView<String> ss, const Size pos) const;

    Size findLast(const String& s) const;

    void replace(const Size pos, const Size n, const String& s);

    void replace(const String& old, const String& s);

    template <typename... TArgs>
    void replace(const String& old, const String& s, TArgs&&... args) {
        replace(old, s);
        replace(std::forward<TArgs>(args)...);
    }

    String substr(const Size pos, const Size n = String::npos) const;

    String trim() const;

    String lower() const;

private:
    bool sanityCheck() {
        return !data.empty() && data.size() < npos / 2 && data[data.size() - 1] == '\0';
    }
};


INLINE bool operator<(const String& s1, const String& s2) {
    return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end());
}

INLINE String operator"" _s(const char* s, std::size_t UNUSED(len)) {
    return String(s);
}

template <typename T>
INLINE String toString(const T& value) {
    std::stringstream ss;
    ss << value;
    return ss.str().c_str();
}


/// Utility functions

/// Returns the current time, formatted by given formatting string.
String getFormattedTime(const String& format);


NAMESPACE_SPH_END
