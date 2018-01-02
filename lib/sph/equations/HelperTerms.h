#pragma once

/// \file HelperTerms.h
/// \brief Additional equatiosn computing SPH statistics rather than physical quantities
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "sph/equations/EquationTerm.h"

NAMESPACE_SPH_BEGIN

/// \brief Helper term counting the number of neighbours of each particle.
class NeighbourCountTerm : public IEquationTerm {
private:
    class NeighbourCountImpl : public DerivativeTemplate<NeighbourCountImpl> {
    private:
        ArrayView<Size> neighCnts;

    public:
        virtual void create(Accumulated& results) override {
            results.insert<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO);
        }

        virtual void initialize(const Storage& UNUSED(input), Accumulated& results) override {
            neighCnts = results.getBuffer<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO);
        }

        template <bool Symmetrize>
        INLINE void eval(const Size i,
            ArrayView<const Size> neighs,
            ArrayView<const Vector> UNUSED_IN_RELEASE(grads)) {
            ASSERT(neighs.size() == grads.size());
            // there is no need to use this in asymmetric solver, since we already know all the neighbours
            ASSERT(Symmetrize);
            neighCnts[i] += neighs.size();
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];
                neighCnts[j]++;
            }
        }
    };

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<NeighbourCountImpl>(settings);
    }

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& UNUSED(storage)) override {}

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
        virtual void create(Accumulated& results) override {
            results.insert<Vector>(QuantityId::SURFACE_NORMAL, OrderEnum::ZERO);
        }

        virtual void initialize(const Storage& input, Accumulated& results) override {
            r = input.getValue<Vector>(QuantityId::POSITION);
            n = results.getBuffer<Vector>(QuantityId::SURFACE_NORMAL, OrderEnum::ZERO);
        }

        template <bool Symmetrize>
        INLINE void eval(const Size i,
            ArrayView<const Size> neighs,
            ArrayView<const Vector> UNUSED_IN_RELEASE(grads)) {
            ASSERT(neighs.size() == grads.size());
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];
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
        }
    };

public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<Derivative>(settings);
    }

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& UNUSED(storage)) override {}

    virtual void create(Storage& storage, IMaterial& UNUSED(material)) const override {
        storage.insert<Vector>(QuantityId::SURFACE_NORMAL, OrderEnum::ZERO, Vector(0._f));
    }
};


NAMESPACE_SPH_END
