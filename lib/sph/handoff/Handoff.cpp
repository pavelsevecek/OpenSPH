#include "sph/handoff/Handoff.h"
#include "objects/finders/NeighbourFinder.h"
#include "objects/geometry/Domain.h"
#include "physics/Integrals.h"
#include "post/Analysis.h"
#include "quantities/Quantity.h"
#include "sph/Materials.h"
#include "sph/initial/Distribution.h"
#include "sph/initial/Initial.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

/// \brief Domain represented by SPH particles
class SphDomain : public IDomain {
private:
    /// Storage holding the particles
    const Storage& storage;

    /// Indices of the particles from which the domain is constructed
    ArrayView<const Size> idxs;

    /// Cached array of particle positions corresponding to the indices in \ref idxs.
    Array<Vector> positions;

    AutoPtr<IBasicFinder> finder;
    LutKernel<3> kernel;
    Float level;

public:
    explicit SphDomain(const Storage& storage, ArrayView<const Size> idxs, const RunSettings& settings)
        : storage(storage)
        , idxs(std::move(idxs)) {
        finder = Factory::getFinder(settings);
        kernel = Factory::getKernel<3>(settings);
        level = 0.15_f; /// \todo generalize

        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        for (Size i : this->idxs) {
            positions.push(r[i]);
        }

        // build the finder only with selected particles
        finder->build(SEQUENTIAL, positions);
    }

    virtual Vector getCenter() const override {
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        Vector r_com(0._f);
        Float m_tot(0._f);
        for (Size i : idxs) {
            m_tot += m[i];
            r_com += m[i] * r[i];
        }
        ASSERT(m_tot != 0._f);
        return r_com / m_tot;
    }

    virtual Box getBoundingBox() const override {
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        Box box;
        for (Size i : idxs) {
            box.extend(r[i] + Vector(r[i][H]));
            box.extend(r[i] - Vector(r[i][H]));
        }
        return box;
    }

    virtual Float getVolume() const override {
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        ArrayView<const Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
        Float volume = 0._f;
        for (Size i : idxs) {
            volume += m[i] / rho[i];
        }
        return volume;
    }

    virtual bool contains(const Vector& v) const override {
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        ArrayView<const Float> rho = storage.getValue<Float>(QuantityId::DENSITY);

        Float h_max = 0._f;
        for (Size i : idxs) {
            h_max = max(h_max, r[i][H]);
        }

        Array<NeighbourRecord> neighs;
        const Float radius = kernel.radius() * h_max;
        finder->findAll(v, radius, neighs);

        Float field = 0._f;
        for (auto& n : neighs) {
            // note that n.index is an index with idxs (or positions) array
            const Size j = idxs[n.index];
            field += m[j] / rho[j] * kernel.value(v - r[j], r[j][H]);
        }
        return field > level;
    }

    virtual void getSubset(ArrayView<const Vector>, Array<Size>&, const SubsetType) const override {
        NOT_IMPLEMENTED;
    }

    virtual void getDistanceToBoundary(ArrayView<const Vector>, Array<Float>&) const override {
        NOT_IMPLEMENTED;
    }

    virtual void project(ArrayView<Vector>, Optional<ArrayView<Size>>) const override {
        NOT_IMPLEMENTED;
    }

    virtual void addGhosts(ArrayView<const Vector>, Array<Ghost>&, const Float, const Float) const override {
        NOT_IMPLEMENTED;
    }
};

static Vector getAngularFrequency(ArrayView<const Size> idxs,
    ArrayView<const Float> m,
    ArrayView<const Vector> r,
    ArrayView<const Vector> v,
    const Vector& r_com,
    const Vector& v_com) {
    // currently Post::getAngularFrequency cannot work with subsets, so we need to duplicate the buffers :(
    Array<Vector> r_lr, v_lr;
    Array<Float> m_lr;
    for (Size i : idxs) {
        r_lr.push(r[i]);
        v_lr.push(v[i]);
        m_lr.push(m[i]);
    }

    return Post::getAngularFrequency(m_lr, r_lr, v_lr, r_com, v_com);
}

Storage convertSphToSpheres(const Storage& sph, const RunSettings& settings, const HandoffParams& params) {
    // clone required quantities
    Array<Vector> r_nbody = sph.getValue<Vector>(QuantityId::POSITION).clone();
    Array<Vector> v_nbody = sph.getDt<Vector>(QuantityId::POSITION).clone();
    Array<Float> m_nbody = sph.getValue<Float>(QuantityId::MASS).clone();

    // radii handoff
    ArrayView<const Float> m_sph = sph.getValue<Float>(QuantityId::MASS);
    ArrayView<const Float> rho_sph = sph.getValue<Float>(QuantityId::DENSITY);
    ASSERT(r_nbody.size() == rho_sph.size());
    for (Size i = 0; i < r_nbody.size(); ++i) {
        switch (params.radius) {
        case HandoffParams::Radius::EQUAL_VOLUME:
            r_nbody[i][H] = cbrt(3._f * m_sph[i] / (4._f * PI * rho_sph[i]));
            break;
        case HandoffParams::Radius::SMOOTHING_LENGTH:
            r_nbody[i][H] = params.radiusMultiplier * r_nbody[i][H];
            break;
        default:
            NOT_IMPLEMENTED;
        }
    }

    // remove all sublimated particles
    /// \todo when to remove? to avoid shifting indices
    /*Array<Size> toRemove;
    ArrayView<const Float> u = sph.getValue<Float>(QuantityId::ENERGY);
    for (Size matId = 0; matId < sph.getMaterialCnt(); ++matId) {
        MaterialView mat = sph.getMaterial(matId);
        for (Size i : mat.sequence()) {
            if (u[i] > params.sublimationEnergy) {
                toRemove.push(i);
            }
        }
    }    */

    if (params.centerOfMassSystem) {
        moveToCenterOfMassSystem(m_nbody, v_nbody);
        moveToCenterOfMassSystem(m_nbody, r_nbody);
    }

    if (params.largestRemnant) {
        Array<Size> idxs = Post::findLargestComponent(sph, 2._f);
        ASSERT(std::is_sorted(idxs.begin(), idxs.end()));

        // find mass, COM and velocity of the largest remnant
        Float m_tot = 0._f;
        Vector r_com(0._f);
        Vector v_com(0._f);
        for (Size i : idxs) {
            m_tot += m_nbody[i];
            v_com += m_nbody[i] * v_nbody[i];
            r_com += m_nbody[i] * r_nbody[i];
        }
        r_com /= m_tot;
        v_com /= m_tot;

        // generate new particles for LR
        const Size particleCnt = params.largestRemnant->particleOverride.valueOr(idxs.size());
        SphDomain domain(sph, idxs, settings);
        Array<Vector> r_lr = params.largestRemnant->distribution->generate(SEQUENTIAL, particleCnt, domain);

        // distribute the total mass uniformly
        Array<Float> m_lr(r_lr.size());
        m_lr.fill(m_tot / r_lr.size());

        // set the velocities as if LR was a rigid body
        Array<Vector> v_lr(r_lr.size());
        const Vector omega = getAngularFrequency(idxs, m_nbody, r_nbody, v_nbody, r_com, v_com);
        for (Size i = 0; i < r_lr.size(); ++i) {
            v_lr[i] = v_com + cross(omega, r_lr[i]);
        }

        // remove all old particles
        r_nbody.remove(idxs);
        v_nbody.remove(idxs);
        m_nbody.remove(idxs);

        // add the new particles
        r_nbody.pushAll(r_lr);
        v_nbody.pushAll(v_lr);
        m_nbody.pushAll(m_lr);
    }


    Storage storage(makeAuto<NullMaterial>(EMPTY_SETTINGS));
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(r_nbody));
    storage.getDt<Vector>(QuantityId::POSITION) = std::move(v_nbody);
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, std::move(m_nbody));
    return storage;
}

NAMESPACE_SPH_END
