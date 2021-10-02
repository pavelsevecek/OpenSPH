#pragma once

/// \file String.h
/// \brief Object representing a sequence of unicode characters
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/ForwardDecl.h"
#include "objects/containers/Array.h"
#include "objects/containers/StaticArray.h"
#include "objects/wrappers/Flags.h"
#include <cstring>

NAMESPACE_SPH_BEGIN

class CharString {
private:
    Array<char> data = { '\0' };

public:
    CharString() = default;

    CharString(const CharString& other)
        : data(other.data.clone()) {}

    CharString(CharString&& other)
        : data(std::move(other.data)) {}

    CharString(ArrayView<const char> wchars) {
        data.insert(0, wchars.begin(), wchars.end());
    }

    CharString& operator=(const CharString& other) {
        data = copyable(other.data);
        return *this;
    }

    CharString& operator=(CharString&& other) {
        data = std::move(other.data);
        return *this;
    }

    char& operator[](const Size idx) {
        SPH_ASSERT(idx < this->size());
        return data[idx];
    }

    char operator[](const Size idx) const {
        SPH_ASSERT(idx < this->size());
        return data[idx];
    }

    Size size() const {
        return data.size() - 1;
    }

    operator char*() {
        return &data[0];
    }

    operator const char*() const {
        return &data[0];
    }

    char* cstr() {
        return &data[0];
    }
};

class String {
private:
    Array<wchar_t> data = { L'\0' };

    String(Array<wchar_t>&& buffer)
        : data(std::move(buffer)) {
        SPH_ASSERT(sanityCheck());
    }

public:
    String() = default;

    String(const String& other)
        : data(other.data.clone()) {}

    String(String&& other) {
        data = std::move(other.data);
    }

    String(const wchar_t* s);

    String(const wchar_t c) {
        data.insert(0, c);
    }

    template <std::size_t N>
    String(const char (&s)[N])
        : String(String::fromAscii(s)) {}

    static String fromAscii(const char* s);

    static String fromUtf8(const char* s);

    static String fromWstring(const std::wstring& wstr) {
        return String(wstr.c_str());
    }

    static String fromChar(const char c, const Size cnt) {
        Array<wchar_t> data;
        for (Size i = 0; i < cnt; ++i) {
            data.push(c);
        }
        data.push(L'\0');
        return data;
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
        SPH_ASSERT(sanityCheck());
        return *this;
    }

    friend String operator+(const String& s1, const String& s2) {
        String s = s1;
        s += s2;
        return s;
    }

    bool operator==(const String& other) const {
        return data == other.data;
    }

    bool operator!=(const String& other) const {
        return data != other.data;
    }

    const wchar_t* toUnicode() const {
        return &data[0];
    }

    CharString toAscii() const;

    CharString toUtf8() const;

    std::wstring toWstring() const;

    bool isAscii() const;

    INLINE wchar_t operator[](const Size idx) const {
        SPH_ASSERT(idx < this->size());
        return data[idx];
    }

    INLINE wchar_t& operator[](const Size idx) {
        SPH_ASSERT(idx < this->size());
        return data[idx];
    }

    INLINE Size size() const {
        return data.size() - 1;
    }

    INLINE bool empty() const {
        return data.size() == 1;
    }

    ArrayView<const wchar_t> view() const {
        return data.view();
    }

    /// \section Iterators

    Iterator<wchar_t> begin() {
        return data.begin();
    }

    Iterator<const wchar_t> begin() const {
        return data.begin();
    }

    Iterator<wchar_t> end() {
        return data.end() - 1;
    }

    Iterator<const wchar_t> end() const {
        return data.end() - 1;
    }

    /// \addtogroup utility Utility functions

    static Size npos;

    Size find(const String& s, const Size pos = 0) const;

    Size find(const wchar_t c, const Size pos = 0) const;

    Size findAny(ArrayView<const String> ss, const Size pos) const;

    Size findAny(ArrayView<const wchar_t> cs, const Size pos) const;

    Size findLast(const String& s) const;

    Size findLast(const wchar_t c) const;

    /// \brief Replaces part of the string with given string.
    ///
    /// \param pos Starting position.
    /// \param n Number of characters to replace. If String::npos, the rest of the string is replaced.
    /// \param s New string to instert into the removed part.
    void replace(const Size pos, const Size n, const String& s);

    /// \brief Replaces the first occurence of given string with another string.
    ///
    /// Returns true if the string was replaced.
    bool replaceFirst(const String& old, const String& s);

    /// \brief Replaces all occurences of given string with another string.
    ///
    /// Returns the number of replacements.
    Size replaceAll(const String& old, const String& s);

    void insert(const Size pos, const String& s);

    void erase(const Size pos, const Size n);

    void clear();

    String substr(const Size pos, const Size n = String::npos) const;

    enum class TrimFlag {
        SPACE = 1 << 0,
        END_LINE = 1 << 1,
        TAB = 1 << 2,
    };

    String trim(const Flags<TrimFlag> flags = TrimFlag::SPACE) const;

    String toLowercase() const;

private:
    bool sanityCheck() {
        return !data.empty() && data.size() < npos / 2 && data[data.size() - 1] == L'\0';
    }
};

template <std::size_t N>
String operator+(const String& s, const char (&c)[N]) {
    return s + String::fromAscii(c);
}

template <std::size_t N>
String operator+(const char (&c)[N], const String& s) {
    return String::fromAscii(c) + s;
}

INLINE bool operator<(const String& s1, const String& s2) {
    return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end());
}

INLINE String operator"" _s(const char* s, std::size_t UNUSED(len)) {
    return String::fromAscii(s);
}

INLINE String operator"" _s(const wchar_t* s, std::size_t UNUSED(len)) {
    return String(s);
}

INLINE std::ostream& operator<<(std::ostream& stream, const String& str) {
    stream << str.toAscii();
    return stream;
}

INLINE std::wostream& operator<<(std::wostream& stream, const String& str) {
    stream << str.toUnicode();
    return stream;
}

INLINE std::wistream& operator>>(std::wistream& stream, String& str) {
    std::wstring wstr;
    stream >> wstr;
    str = wstr.c_str();
    return stream;
}

template <typename T>
INLINE String toString(const T& value) {
    std::wstringstream ss;
    ss << value;
    return ss.str().c_str();
}

class FormatException : public std::exception {
private:
    CharString message;

public:
    explicit FormatException(const String& f)
        : message(("Failed to format a string '" + f + "'").toUtf8()) {}

    virtual const char* what() const noexcept {
        return message;
    }
};

template <typename TFirst, typename... TRest>
String format(String f, const TFirst& first, const TRest&... rest) {
    static_assert(!IsStringLiteral<TFirst>::value, "Format parameter shall not be a string literal");
    if (!f.replaceFirst("{}", toString(first))) {
        throw FormatException(f);
    }
    return format(std::move(f), rest...);
}

inline String format(String f) {
    if (f.find("{}") != String::npos) {
        throw FormatException(f);
    }
    return f;
}

template <typename T>
Optional<T> fromString(const String& s);


/// Utility functions

/// \brief Returns the message of given exception as \ref String.
String exceptionMessage(const std::exception& e);

/// \brief Inserts line break to given string so that the line widths are below given value.
String setLineBreak(const String& s, const Size lineWidth);

Array<String> split(const String& s, const wchar_t delimiter);

Pair<String> splitByFirst(const String& s, const wchar_t delimiter);

/// \brief Makes first letters of each word upper-cases, except for words like 'and', 'of', etc.
String capitalize(const String& s);

NAMESPACE_SPH_END
