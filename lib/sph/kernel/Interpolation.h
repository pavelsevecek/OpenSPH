#pragma once

/// \file Interpolation.h
/// \brief Computes interpolated values of quantities between SPH particles
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/finders/NeighbourFinder.h"
#include "objects/wrappers/SharedPtr.h"
#include "quantities/Storage.h"
#include "sph/kernel/KernelFactory.h"
#include "system/Factory.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

/// \brief Object for computing interpolated values of quantities
///
/// Quantity values or derivatives are interpolated using SPH kernel. If the point of interpolation
/// lies outside of support of all SPH particles, the interpolated value is zero.
class Interpolation : public Noncopyable {
private:
    AutoPtr<IBasicFinder> finder;

    LutKernel<3> kernel;

    Float searchRadius;

    const Storage& storage;

public:
    /// \brief Constructs the interpolation object from settings.
    Interpolation(const Storage& storage, const RunSettings& settings = RunSettings::getDefaults())
        : finder(Factory::getFinder(settings))
        , kernel(Factory::getKernel<3>(settings))
        , storage(storage) {
        this->build();
    }

    /// \brief Constructs the interpolation object given a neighbour finding object and a SPH kernel.
    /// \param finder Object for finding neighbours. Overload of INeighbourFinder::findNeighbours taking a
    ///               vector must be implemented. Parameter must not be nullptr.
    /// \param kernel SPH kernel used for interpolation.
    Interpolation(const Storage& storage, AutoPtr<IBasicFinder>&& finder, LutKernel<3>&& kernel)
        : finder(std::move(finder))
        , kernel(std::move(kernel))
        , storage(storage) {
        ASSERT(this->finder);
        this->build();
    }

    /// \brief Computes interpolated value at given point.
    /// \tparam Type Value type of the quantity, must be the type of the quantity being interpolated.
    /// \param id Quantity values of which we wish to interpolate. The quantity must be stored in the storage.
    /// \param deriv Specifies whether to interpolate quantity values or its derivatives. For values, use
    ///              OrderEnum::ZERO. If the first or second derivative is being interpolated, the quantity
    ///              must be of corresponding order, checked by assert.
    /// \param pos Position where the interpolated value is computed.
    /// \return Interpolated value
    template <typename Type>
    Type interpolate(const QuantityId id, const OrderEnum deriv, const Vector& pos) const {
        /// \todo measure these getters
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m, rho;
        tie(m, rho) = storage.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY);
        ArrayView<const Type> quantity = storage.getAll<Type>(id)[int(deriv)];

        Type interpol(0._f);
        Array<NeighbourRecord> neighs;
        finder->findAll(pos, kernel.radius() * searchRadius, neighs);
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k].index;
            if (neighs[k].distanceSqr < sqr(kernel.radius() * r[j][H])) {
                interpol += m[j] / rho[j] * quantity[j] * kernel.value(pos - r[j], r[j][H]);
            }
        }
        return interpol;
    }

private:
    void build() {
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        finder->build(r);
        searchRadius = 0._f;
        for (Size i = 0; i < r.size(); ++i) {
            searchRadius = max(searchRadius, r[i][H]);
        }
    }
};

NAMESPACE_SPH_END
