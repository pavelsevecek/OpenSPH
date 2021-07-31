#pragma once

/// \file Stellar.h
/// \brief Initial conditions of a polytropic star
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/wrappers/Lut.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

class IEos;

namespace Stellar {

/// \brief Solves the Lane-Emden equation given the polytrope index.
Lut<Float> solveLaneEmden(const Float n, const Float dz = 1.e-3_f, const Float z_max = 1.e3_f);

struct Star {
    Lut<Float> rho;
    Lut<Float> u;
    Lut<Float> p;
};

/// \brief Computes radial profiles of state quantities for a polytropic star.
Star polytropicStar(const IEos& eos, const Float radius, const Float mass, const Float n);

/// \brief Creates a spherical polytropic star.
///
/// \param scheduler Scheduler used for parallelization
/// \param material Material containing the equation of state to use.
/// \param distribution Distribution used to generate particles
/// \param particleCnt Number of particles
/// \param radius Radius of the star
/// \param mass Total mass of the star
Storage generateIc(const SharedPtr<IScheduler>& scheduler,
    const SharedPtr<IMaterial>& material,
    const IDistribution& distribution,
    const Size particleCnt,
    const Float radius,
    const Float mass);

} // namespace Stellar

NAMESPACE_SPH_END
