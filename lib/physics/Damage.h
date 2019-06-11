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

/// Scalar damage describing fragmentation of the body according to Grady-Kipp model (Grady and Kipp, 1980)
class ScalarGradyKippModel : public IFractureModel {
public:
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

class NullFracture : public IFractureModel {
public:
    virtual void setFlaws(Storage& storage,
        IMaterial& material,
        const MaterialInitialContext& context) const override;

    virtual void integrate(IScheduler& scheduler, Storage& storage, const MaterialView material) override;
};

NAMESPACE_SPH_END
