#pragma once

/// \file HelperTerms.h
/// \brief Additional equation terms computing SPH statistics rather than physical quantities
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "io/FileSystem.h"
#include "run/ScriptUtils.h"
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

    virtual void initialize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

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

    virtual void initialize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void create(Storage& storage, IMaterial& UNUSED(material)) const override {
        storage.insert<Vector>(QuantityId::SURFACE_NORMAL, OrderEnum::ZERO, Vector(0._f));
    }
};

class ChaiScriptTerm : public IEquationTerm {
private:
    std::string script;
    Float period;
    bool oneShot;
    bool done;
    Float nextTime;

#ifdef SPH_USE_CHAISCRIPT
    Chai::Particles particles;
#endif

public:
    explicit ChaiScriptTerm(const Path& scriptFile, const Float period, const bool oneShot)
        : period(period)
        , oneShot(oneShot) {
        nextTime = period;
        done = false;
#ifdef SPH_USE_CHAISCRIPT
        script = FileSystem::readFile(scriptFile);
#else
        (void)scriptFile;
        throw InvalidSetup("Code not built with ChaiScript support. Re-build with flag 'use_chaiscript'.");
#endif
    }

    virtual void setDerivatives(DerivativeHolder& UNUSED(derivatives),
        const RunSettings& UNUSED(settings)) override {}

    virtual void initialize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler), Storage& storage, const Float t) override {
#ifdef SPH_USE_CHAISCRIPT
        if (t < nextTime || done) {
            return;
        }
        nextTime += period;
        particles.bindToStorage(storage);

        chaiscript::ChaiScript chai;
        Chai::registerBindings(chai);
        chai.add(chaiscript::var(std::ref(particles)), "particles");
        chai.add(chaiscript::const_var(t), "time");
        chai.eval(script);
        particles.store();

        if (oneShot) {
            done = true;
        }
#else
        (void)storage;
        (void)t;
        (void)period;
        (void)oneShot;
#endif
    }

    virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
};


NAMESPACE_SPH_END
