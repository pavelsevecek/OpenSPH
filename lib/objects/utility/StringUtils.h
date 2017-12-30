#pragma once

#include "objects/containers/Array.h"
#include "objects/wrappers/Optional.h"
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
std::string replace(const std::string& source, const std::string& old, const std::string& s);

/// Splits a string into an array of string using given delimiter.
Array<std::string> split(const std::string& s, const char delimiter);

NAMESPACE_SPH_END