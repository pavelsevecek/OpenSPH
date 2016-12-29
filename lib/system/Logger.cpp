#include "system/Logger.h"
#include <iostream>

NAMESPACE_SPH_BEGIN

void StdOutLogger::write(const std::string& s) { std::cout << s << std::endl; }

NAMESPACE_SPH_END
