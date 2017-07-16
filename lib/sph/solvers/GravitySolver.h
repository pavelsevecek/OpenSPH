#pragma once

/// \file GravitySolver.h
/// \brief SPH solver including gravity
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gravity/BarnesHut.h"
#include "sph/kernel/KernelFactory.h"
#include "sph/solvers/GenericSolver.h"

NAMESPACE_SPH_BEGIN

/// Extension of simple N-body Barnes-Hut gravity for SPH, using smoothing kernel
class SmoothedBarnesHut : public BarnesHut {
private:
    GravityLutKernel gravityKernel;

public:
    virtual void buildLeaf(KdNode& node) override {
        LeafNode& leaf = (LeafNode&)node;
        // extend bounding boxes by particle radius
        LeafIndexSequence sequence = kdTree.getLeafIndices(leaf);
        for (Size i : sequence) {
            ASSERT(isReal(r[i][H]));
            const Vector dr(r[i][H] * gravityKernel.closeRadius());
            leaf.box.extend(r[i] + dr);
            leaf.box.extend(r[i] - dr);
        }
    }
};

/// \brief Gravity solver
class GravitySolver : public GenericSolver {
private:
    BarnesHut gravity;

    SymmetrizeValues<GravityLutKernel> gravityKernel;

    /// Dummy term to make sure acceleration is being accumulated
    struct DummyAcceleration : public Abstract::EquationTerm {
        struct DummyDerivative : public Abstract::Derivative {
            virtual void create(Accumulated& results) override {
                results.insert<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND);
            }
            virtual void initialize(const Storage&, Accumulated&) override {}
            virtual void evalSymmetric(const Size, ArrayView<const Size>, ArrayView<const Vector>) override {}
            virtual void evalAsymmetric(const Size, ArrayView<const Size>, ArrayView<const Vector>) override {
            }
        };

        virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
            derivatives.require<DummyDerivative>(settings);
        }
        virtual void initialize(Storage&) override {}
        virtual void finalize(Storage&) override {}
        virtual void create(Storage&, Abstract::Material&) const override {}
    };

public:
    GravitySolver(const RunSettings& settings, const EquationHolder& equations)
        : GenericSolver(settings, equations + makeTerm<DummyAcceleration>())
        , gravity(settings.get<Float>(RunSettingsId::GRAVITY_OPENING_ANGLE), MultipoleOrder::OCTUPOLE) {
        gravityKernel = Factory::getGravityKernel(settings);
    }

protected:
    virtual void loop(Storage& storage) override {
        // first, do asymmetric evaluation of gravity
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
        ArrayView<Float> m = storage.getValue<Float>(QuantityId::MASSES);
        gravity.eval(r[0]);
        MARK_USED(m);

        // second, compute SPH derivatives using symmetric evaluation
        GenericSolver::loop(storage);
    }
};

NAMESPACE_SPH_END
