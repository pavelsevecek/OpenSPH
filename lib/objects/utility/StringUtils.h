#pragma once

#include "objects/containers/Array.h"
#include "objects/containers/StaticArray.h"
#include "objects/wrappers/Optional.h"
#include <set>
#include <string>

NAMESPACE_SPH_BEGIN

/// Converts a string to given type.
template <typename T>
Optional<T> fromString(const std::string& s);

/// Removes all leading and trailing spaces from a string.
std::string trim(const std::string& s);

/// Converts all uppercase characters to their lowercase variants. Other characters are unchanged.
std::string lowercase(const std::string& s);

/// Replaces first occurence of string with a new string.
std::string replaceFirst(const std::string& source, const std::string& old, const std::string& s);

/// Replaces all occurences of string with a new string
std::string replaceAll(const std::string& source, const std::string& old, const std::string& s);

/// Inserts \n to string so that no line is longer than given limit
std::string setLineBreak(const std::string& s, const Size lineWidth);

/// Splits a string into an array of string using given delimiter.
Array<std::string> split(const std::string& s, const char delimiter);

/// Splits a string into two parts, using first occurence of given delimiter.
Pair<std::string> splitByFirst(const std::string& s, const char delimiter);

/// Capitalizes first letters of all words in the string, except for words like 'and', 'or', etc.
std::string capitalize(const std::string& s);

class UniqueNameManager {
private:
    std::set<std::string> names;

public:
    UniqueNameManager() = default;

    explicit UniqueNameManager(ArrayView<const std::string> initial);

    std::string getName(const std::string& name);
};

NAMESPACE_SPH_END
