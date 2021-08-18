#pragma once

#include "io/Path.h"

NAMESPACE_SPH_BEGIN

#ifndef SPH_CONFIG_SET
static_assert(false,
    "You need to modify these paths according to your system. Then define macro SPH_CONFIG_SET and rebuild "
    "the tests.");
#else

#ifndef SPH_WIN
/// Directory containing auxiliary resources for tests (serialized storage, etc.)
#ifdef SPH_TEST_RESOURCE_PATH
const Path RESOURCE_PATH = Path(SPH_STR(SPH_TEST_RESOURCE_PATH));
#else
const Path RESOURCE_PATH = Path("/home/pavel/projects/astro/sph/src/test/resources");
#endif

/// Home directory of the user running the tests
const Path HOME_DIR = Path("/home/pavel/");

/// Working directory of the executable, used for testing of Path class.
#ifdef SPH_DEBUG
const Path WORKING_DIR = Path("/home/pavel/projects/astro/sph/build-debug/test/");
#else
const Path WORKING_DIR = Path("/home/pavel/projects/astro/sph/build-release/test/");
#endif

/// Directory containing auxiliary resources for tests (serialized storage, etc.)
const Path RESOURCE_PATH = Path("/home/pavel/projects/astro/sph/src/test/resources/");

#else

const Path HOME_DIR = Path("C:/Users/pavel/");
const Path WORKING_DIR = Path("D:/projects/astro/sph/build/test/");
const Path RESOURCE_PATH = Path("D:/projects/astro/sph/src/test/resources/");

#endif

#endif

NAMESPACE_SPH_END
