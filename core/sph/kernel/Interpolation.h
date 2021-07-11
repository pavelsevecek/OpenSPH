#pragma once

/// \file Interpolation.h
/// \brief Computes interpolated values of quantities between SPH particles
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/finders/NeighbourFinder.h"
#include "objects/wrappers/SharedPtr.h"
#include "quantities/Storage.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "system/Settings.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

/// \brief Interface for computing quantity at any point in space.
/// \tparam Type Value type of the quantity being interpolated.
template <typename Type>
class IInterpolant : public Noncopyable {
public:
    /// \brief Compute quantity value at given position.
    /// \param pos Position where the interpolated value is computed.
    /// \return Interpolated value
    virtual Type interpolate(const Vector& pos) const = 0;
};

/// \brief Object for computing interpolated values of quantities.
///
/// Quantity values or derivatives are interpolated using SPH kernel. If the point of interpolation
/// lies outside of support of all SPH particles, the interpolated value is zero.
/// \tparam Type Value type of the quantity being interpolated.
template <typename Type>
class SphInterpolant : public IInterpolant<Type> {
protected:
    ArrayView<const Vector> r;
    ArrayView<const Float> m, rho;
    ArrayView<const Type> quantity;

    AutoPtr<IBasicFinder> finder;
    LutKernel<3> kernel;
    Float searchRadius = 0._f;

public:
    /// \brief Constructs the interpolation object from settings.
    SphInterpolant(const Storage& storage,
        const QuantityId id,
        const OrderEnum deriv,
        const RunSettings& settings = RunSettings::getDefaults())
        : SphInterpolant(storage, id, deriv, Factory::getFinder(settings), Factory::getKernel<3>(settings)) {}

    /// \brief Constructs the interpolation object given a neighbour finding object and a SPH kernel.
    /// \param finder Object for finding neighbours. Overload of INeighbourFinder::findNeighbours taking a
    ///               vector must be implemented. Parameter must not be nullptr.
    /// \param kernel SPH kernel used for interpolation.
    /// \param id Quantity values of which we wish to interpolate. The quantity must be stored in the storage.
    /// \param deriv Specifies whether to interpolate quantity values or its derivatives. For values, use
    ///              OrderEnum::ZERO. If the first or second derivative is being interpolated, the quantity
    ///              must be of corresponding order, checked by assert.
    SphInterpolant(const Storage& storage,
        const QuantityId id,
        const OrderEnum deriv,
        AutoPtr<IBasicFinder>&& finder,
        LutKernel<3>&& kernel)
        : finder(std::move(finder))
        , kernel(std::move(kernel)) {
        SPH_ASSERT(this->finder);
        r = storage.getValue<Vector>(QuantityId::POSITION);
        tie(m, rho) = storage.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY);
        quantity = storage.getAll<Type>(id)[int(deriv)];
        this->build(storage);
    }

    virtual Type interpolate(const Vector& pos) const override {
        Type interpol(0._f);
        Array<NeighbourRecord> neighs;
        finder->findAll(pos, searchRadius, neighs);
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k].index;
            if (neighs[k].distanceSqr < sqr(kernel.radius() * r[j][H])) {
                interpol += m[j] / rho[j] * quantity[j] * kernel.value(pos - r[j], r[j][H]);
            }
        }
        return interpol;
    }

private:
    void build(const Storage& storage) {
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        finder->build(SEQUENTIAL, r);
        searchRadius = 0._f;
        for (Size i = 0; i < r.size(); ++i) {
            searchRadius = max(searchRadius, kernel.radius() * r[i][H]);
        }
    }
};

/// \brief Computes corrected interpolated value.
///
/// The interpolated quantity is divided by a constant 1 interpolated using SPH kernel. The result thus does
/// not decrease to zero at the domain boundary, instead there is a discontinuity of the interpolant.
template <typename Type>
class CorrectedSphInterpolant : public SphInterpolant<Type> {
public:
    using SphInterpolant<Type>::SphInterpolant;

    virtual Type interpolate(const Vector& pos) const override {
        Type interpol(0._f);
        Float weight = 0._f;
        Array<NeighbourRecord> neighs;
        this->finder->findAll(pos, this->searchRadius, neighs);
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k].index;
            if (neighs[k].distanceSqr < sqr(this->kernel.radius() * this->r[j][H])) {
                const Float w =
                    this->m[j] / this->rho[j] * this->kernel.value(pos - this->r[j], this->r[j][H]);
                interpol += this->quantity[j] * w;
                weight += w;
            }
        }
        if (weight > 0._f) {
            return interpol / weight;
        } else {
            return Type(0._f);
        }
    }
};

NAMESPACE_SPH_END
