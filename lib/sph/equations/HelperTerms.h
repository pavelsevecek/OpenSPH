#pragma once

/// \file HelperTerms.h
/// \brief Additional equatiosn computing SPH statistics rather than physical quantities
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "sph/equations/DerivativeHelpers.h"
#include "sph/equations/EquationTerm.h"

NAMESPACE_SPH_BEGIN

/// \brief Helper term counting the number of neighbours of each particle.
class NeighbourCountTerm : public IEquationTerm {
private:
    class Derivative : public DerivativeTemplate<Derivative> {
    private:
        ArrayView<Size> neighCnts;

    public:
        explicit Derivative(const RunSettings& settings)
            : DerivativeTemplate<Derivative>(settings) {}

        INLINE void additionalCreate(Accumulated& results) {
            results.insert<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO, BufferSource::UNIQUE);
        }

        INLINE void additionalInitialize(const Storage& UNUSED(input), Accumulated& results) {
            neighCnts = results.getBuffer<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO);
        }

        INLINE bool additionalEquals(const Derivative& UNUSED(other)) const {
            return true;
        }

        template <bool Symmetrize>
        INLINE void eval(const Size i, const Size j, const Vector& UNUSED(grad)) {
            // there is no need to use this in asymmetric solver, since we already know all the neighbours
            ASSERT(Symmetrize);
            neighCnts[i]++;
            if (Symmetrize) {
                neighCnts[j]++;
            }
        }
    };

public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require(makeAuto<Derivative>(settings));
    }

    virtual void initialize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) override {}

    virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
};


/// \brief Term computing normals of free surface.
///
/// For particles inside the bodies, this will result to vectors close to zero (depending on the number of
/// neighbours), and the term can be therefore used to detect boundary particles
class SurfaceNormal : public IEquationTerm {
private:
    class Derivative : public DerivativeTemplate<Derivative> {
    private:
        ArrayView<const Vector> r;
        ArrayView<Vector> n;

    public:
        explicit Derivative(const RunSettings& settings)
            : DerivativeTemplate<Derivative>(settings) {}

        INLINE void additionalCreate(Accumulated& results) {
            results.insert<Vector>(QuantityId::SURFACE_NORMAL, OrderEnum::ZERO, BufferSource::UNIQUE);
        }

        INLINE void additionalInitialize(const Storage& input, Accumulated& results) {
            r = input.getValue<Vector>(QuantityId::POSITION);
            n = results.getBuffer<Vector>(QuantityId::SURFACE_NORMAL, OrderEnum::ZERO);
        }

        INLINE bool additionalEquals(const Derivative& UNUSED(other)) const {
            return true;
        }

        template <bool Symmetrize>
        INLINE void eval(const Size i, const Size j, const Vector& UNUSED(grad)) {
            const Vector dr = (r[j] - r[i]) / (r[i][H] + r[j][H]);
            const Float length = getLength(dr);
            if (length != 0) {
                const Vector normalized = dr / length;
                n[i] += normalized;
                if (Symmetrize) {
                    n[j] -= normalized;
                }
            }
        }
    };

public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require(makeAuto<Derivative>(settings));
    }

    virtual void initialize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) override {}

    virtual void create(Storage& storage, IMaterial& UNUSED(material)) const override {
        storage.insert<Vector>(QuantityId::SURFACE_NORMAL, OrderEnum::ZERO, Vector(0._f));
    }
};


NAMESPACE_SPH_END
