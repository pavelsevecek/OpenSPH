#pragma once

#include "common/Assert.h"
#include <string>

NAMESPACE_SPH_BEGIN

/// Removes all leading and trailing spaces from a string.
std::string trim(const std::string& s);

/// Converts all uppercase characters to their lowercase variants. Other characters are unchanged.
std::string lowercase(const std::string& s);

/// Replaces first occurence of string with a new string.
std::string replace(const std::string& source, const std::string& old, const std::string& s);

NAMESPACE_SPH_END
