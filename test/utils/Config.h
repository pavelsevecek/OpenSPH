#pragma once

#include "io/Path.h"

NAMESPACE_SPH_BEGIN

#ifdef SPH_DEBUG
const Path WORKING_DIR = Path("/home/pavel/projects/astro/sph/build-debug/test/");
#else
const Path WORKING_DIR = Path("/home/pavel/projects/astro/sph/build-release/test/");
#endif

NAMESPACE_SPH_END
