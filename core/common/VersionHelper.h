#pragma once

#include "Globals.h"
#include "objects/containers/String.h"

NAMESPACE_SPH_BEGIN

static String getEnabledFeatures() {
    String desc;
#ifdef SPH_DEBUG
    desc += "Debug build\n";
#else
    desc += "Release build\n";
#endif

#ifdef SPH_PROFILE
    desc += "Profiling enabled\n";
#endif

#ifdef SPH_USE_TBB
    desc += "Parallelization: TBB\n";
#elif SPH_USE_OPENMP
    desc += "Parallelization: OpenMP\n";
#else
    desc += "Parallelization: built-in thread pool\n";
#endif

#ifdef SPH_USE_EIGEN
    desc += "Eigen: enabled\n";
#else
    desc += "Eigen: disabled\n";
#endif

#ifdef SPH_USE_VDB
    desc += "OpenVDB: enabled\n";
#else
    desc += "OpenVDB: disabled\n";
#endif

#ifdef SPH_USE_CHAISCRIPT
    desc += "Chaiscript: enabled";
#else
    desc += "Chaiscript: disabled";
#endif

    return desc;
}

NAMESPACE_SPH_END
