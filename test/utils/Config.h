#pragma once

#include "io/Path.h"

NAMESPACE_SPH_BEGIN

#ifndef SPH_CONFIG_SET
static_assert(false,
    "You need to modify these paths according to your system. Then define macro SPH_CONFIG_SET and rebuild "
    "the tests.");
#else

/// Directory containing auxiliary resources for tests (serialized storage, etc.)
const Path RESOURCE_PATH = Path("/home/pavel/projects/astro/sph/src/test/resources");

/// Working directory of the executable, used for testing of Path class.
#ifdef SPH_DEBUG
const Path WORKING_DIR = Path("/home/pavel/projects/astro/sph/build-debug/test/");
#else
const Path WORKING_DIR = Path("/home/pavel/projects/astro/sph/build-release/test/");
#endif
#endif

NAMESPACE_SPH_END
