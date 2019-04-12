#pragma once

/// \file Fracture.h
/// \brief Models of fragmentation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "common/ForwardDecl.h"
#include "objects/containers/ArrayView.h"
#include "objects/geometry/TracelessTensor.h"
#include "objects/geometry/Vector.h"
#include "objects/wrappers/Flags.h"

NAMESPACE_SPH_BEGIN

struct MaterialInitialContext;
class IScheduler;

/// \brief Interface representing a fragmentation model.
class IFractureModel : public Polymorphic {
public:
    /// \brief Sets up all the necessary quantities in the storage given material settings.
    virtual void setFlaws(Storage& storage,
        IMaterial& material,
        const MaterialInitialContext& context) const = 0;

    /// \brief Compute damage derivatives
    virtual void integrate(IScheduler& scheduler, Storage& storage, const MaterialView sequence) = 0;
};


enum class ExplicitFlaws {
    /// Distribute flaws uniformly (to random particles), see Benz & Asphaug (1994) \cite Benz_Asphaug_1994,
    /// Sec. 3.3.1
    UNIFORM,

    /// Explicitly assigned flaws by user, used mainly for testing purposes. Values must be set in quantity
    ASSIGNED,
};

/// Scalar damage describing fragmentation of the body according to Grady-Kipp model (Grady and Kipp, 1980)
class ScalarGradyKippModel : public IFractureModel {
private:
    ExplicitFlaws options;

public:
    explicit ScalarGradyKippModel(const ExplicitFlaws options = ExplicitFlaws::UNIFORM);

    virtual void setFlaws(Storage& storage,
        IMaterial& material,
        const MaterialInitialContext& context) const override;

    virtual void integrate(IScheduler& scheduler, Storage& storage, const MaterialView material) override;
};

class TensorGradyKippModel : public IFractureModel {
public:
    virtual void setFlaws(Storage& storage,
        IMaterial& material,
        const MaterialInitialContext& context) const override;

    virtual void integrate(IScheduler& scheduler, Storage& storage, const MaterialView material) override;
};

class MohrCoulombModel : public IFractureModel {
public:
    virtual void setFlaws(Storage& storage,
        IMaterial& material,
        const MaterialInitialContext& context) const override;

    virtual void integrate(IScheduler& scheduler, Storage& storage, const MaterialView material) override;
};

class NullFracture : public IFractureModel {
public:
    virtual void setFlaws(Storage& storage,
        IMaterial& material,
        const MaterialInitialContext& context) const override;

    virtual void integrate(IScheduler& scheduler, Storage& storage, const MaterialView material) override;
};

NAMESPACE_SPH_END
