#include "objects/utility/Dynamic.h"

NAMESPACE_SPH_BEGIN

Dynamic::Dynamic() = default;

/// Needs to be in .cpp to compile with clang, for some reason
Dynamic::~Dynamic() = default;

NAMESPACE_SPH_END
