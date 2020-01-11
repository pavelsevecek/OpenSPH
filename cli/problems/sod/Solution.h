#pragma once

#include "math/Functional.h"
#include "math/MathUtils.h"
#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

class Storage;

struct SodConfig {
    Float x0 = 0._f;
    Float rho_l = 1._f;
    Float P_l = 1._f;
    Float u_l = 0._f;

    Float rho_r = 0.125_f;
    Float P_r = 0.1_f;
    Float u_r = 0._f;

    Float gamma = 1.4_f;
};

/// Analytical solutions of the Sod shock tube
/// http://www.phys.lsu.edu/~tohline/PHYS7412/sod.html
Storage analyticSod(const SodConfig& sod, const Float t);

NAMESPACE_SPH_END
