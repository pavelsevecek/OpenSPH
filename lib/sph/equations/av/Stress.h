#pragma once

/// \file Stress.h
/// \brief Form of tensor artificial viscosity for SPH with stress tensor
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"

NAMESPACE_SPH_BEGIN

/// \brief Artificial stress for simulations including stress tensor
///
/// This artificial force attemps to resolve problems with tensile instability in SPH. It isn't a replacement
/// of 'standard' artificial viscosity, both terms serve different purposes and they complement each other.
/// The implementation more or less follows paper 'SPH without a tensile instability' by Monaghan \cite
/// Monaghan_1999.
///
/// \note This object cannot be used within Balsara switch
class StressAV : public IEquationTerm {
private:
    LutKernel<3> kernel;

    template <typename TDiscr>
    class Derivative;

public:
    StressAV(const RunSettings& settings);

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override;

    virtual void initialize(IScheduler& scheduler, Storage& storage, const Float t) override;

    virtual void finalize(IScheduler& scheduler, Storage& storage, const Float t) override;

    virtual void create(Storage& storage, IMaterial& material) const override;
};

NAMESPACE_SPH_END
