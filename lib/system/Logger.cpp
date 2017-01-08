#include "system/Logger.h"
#include <iostream>

NAMESPACE_SPH_BEGIN

void StdOutLogger::writeString(const std::string& s) { std::cout << s << std::flush; }

NAMESPACE_SPH_END
