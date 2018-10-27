#include "gravity/AggregateSolver.h"
#include "gravity/Collision.h"
#include "gravity/IGravity.h"
#include "post/Analysis.h"
#include "quantities/Storage.h"
#include "system/Factory.h"
#include "system/Statistics.h"
#include "thread/CheckFunction.h"
#include <mutex>

NAMESPACE_SPH_BEGIN

/// \brief Aggregate of particles, moving as a rigid body according to Euler's equations.
///
/// \todo
/// Not owning the particles, bound to a \ref Storage object. Not compatible with mergers.
class Aggregate : public Noncopyable {
private:
    RawPtr<Storage> storage;

    /// Indices of particles belonging to this aggregate
    /// \todo check performance compared to std::set
    FlatSet<Size> idxs;

    /// Index of the aggregate. Does not generally correspond to the index of the aggregate in the parent \ref
    /// AggregateHolder.
    Size persistentId;

public:
    /// Needed due to resize
    Aggregate() = default;

    explicit Aggregate(Storage& storage, const Size particleIdx)
        : storage(addressOf(storage)) {
        idxs.insert(particleIdx);
        persistentId = particleIdx;
    }

    void merge(const Aggregate& other) {
        ASSERT(contains(getId()));
        ASSERT(other.contains(other.getId()));

#ifdef SPH_DEBUG
        Size n1 = size();
        Size n2 = other.size();
#endif
        // preserve ID of the aggregate containing more particles
        if (other.size() > this->size()) {
            persistentId = other.persistentId;
        }

        // add all particles of the other aggregate
        ASSERT(!other.idxs.empty());
        for (Size idx : other.idxs) {
            ASSERT(!contains(idx), idx);
            idxs.insert(idx);
        }
        ASSERT(size() == n1 + n2);
    }

    void clear() {
        idxs.clear();
    }

    bool contains(const Size idx) const {
        return idxs.find(idx) != idxs.end();
    }

    Size getId() const {
        return persistentId;
    }

    /// Called at the beginning of time step
    ///
    /// Assigns particle velocities using COM velocity and bulk rotation
    void initialize() {

        ArrayView<Vector> a = storage->getValue<Vector>(QuantityId::PHASE_ANGLE);

        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m = storage->getValue<Float>(QuantityId::MASS);

        /// \todo compute COM once and cache?
        Float m_ag = 0._f;
        Vector r_com(0._f);
        for (Size i : idxs) {
            r_com += m[i] * r[i];
            m_ag += m[i];
        }
        r_com /= m_ag;

        for (Size i : idxs) {
            r[i] += cross(a[i], r[i] - r_com);
            a[i] = Vector(0._f);
        }
    }

    /// Velocities back to COM + rotation
    void finalize() {
        /*for (Size i : idxs) {
            v[i] = v_com;
            dv[i] = dv_com;
        } */
    }

    /// Called after accelerations are computed
    ///
    /// \todo better name
    void integrate() {
        if (this->size() == 1) {
            return;
        }
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m = storage->getValue<Float>(QuantityId::MASS);

        Float m_ag = 0._f;
        Vector r_com(0._f);
        Vector v_com(0._f);
        Vector dv_com(0._f);
        for (Size i : idxs) {
            dv_com += m[i] * dv[i];
            v_com += m[i] * v[i];
            r_com += m[i] * r[i];
            m_ag += m[i];
        }
        dv_com /= m_ag;
        v_com /= m_ag;
        r_com /= m_ag;

        Vector L(0._f);
        SymmetricTensor I = SymmetricTensor::null(); // inertia tensor
        Vector M(0._f);                              // torque
        for (Size i : idxs) {
            const Vector dr = r[i] - r_com;
            L += m[i] * cross(dr, v[i] - v_com);
            I += m[i] * (SymmetricTensor::identity() * getSqrLength(dr) - outer(dr, dr));
            M += m[i] * cross(dr, dv[i] - dv_com);
        }

        // compute local frame
        const AffineMatrix Em = eigenDecomposition(I).vectors;

        Vector M_loc = Em.transpose() * M;

        ArrayView<Vector> w, dw;
        tie(w, dw) = storage->getAll<Vector>(QuantityId::ANGULAR_FREQUENCY);

        // we save the angular accelerations into the first particle in the aggregate
        const Size index = idxs[0];
        /*if (w[i] == Vector(0._f)) {
            continue;
        }*/
        SymmetricTensor Iinv = I.inverse();
        // convert the angular frequency to the local frame;
        // note the E is always orthogonal, so we can use transpose instead of inverse
        ASSERT(Em.isOrthogonal());
        const Vector w_loc = Em.transpose() * w[index];

        // compute the change of angular velocity using Euler's equations
        const Vector dw_loc = Iinv * (M_loc - cross(w_loc, I * w_loc));

        // now dw[i] is also in local space, convert to inertial space
        // (note that dw_in = dw_loc as omega x omega is obviously zero)
        dw[index] = Em * dw_loc;


        /*const SymmetricTensor I = Post::getInertiaTensor(m, r, r_com);
        (void)I;*/

        for (Size i : idxs) {
            v[i] = v_com;
            dv[i] = dv_com;
        }
    }

    Float mass() const {
        ArrayView<const Float> m = storage->getValue<Float>(QuantityId::MASS);
        Float m_ag = 0._f;
        for (Size i : idxs) {
            m_ag += m[i];
        }
        return m_ag;
    }

    Vector velocity() const {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m = storage->getValue<Float>(QuantityId::MASS);

        Float m_ag = 0._f;
        Vector v_com(0._f);
        for (Size i : idxs) {
            v_com += m[i] * v[i];
            m_ag += m[i];
        }
        v_com /= m_ag;
        return v_com;
    }

    void setVelocity(const Vector& newV) {
        ArrayView<Vector> v = storage->getDt<Vector>(QuantityId::POSITION);
        for (Size i : idxs) {
            v[i] = newV;
        }
    }

    Size size() const {
        return idxs.size();
    }

    auto begin() const {
        return idxs.begin();
    }

    auto end() const {
        return idxs.end();
    }

private:
    struct Integrals {
        Float m;
        Vector r_com;
        Vector v_com;
        Vector omega;
        SymmetricTensor I;
        SymmetricTensor L;
    };

    Integrals getIntegrals() const {
        ASSERT(this->size() > 1);
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m = storage->getValue<Float>(QuantityId::MASS);

        Float m_ag = 0._f;
        Vector r_com(0._f);
        Vector v_com(0._f);
        Vector dv_com(0._f);
        for (Size i : idxs) {
            dv_com += m[i] * dv[i];
            v_com += m[i] * v[i];
            r_com += m[i] * r[i];
            m_ag += m[i];
        }
        dv_com /= m_ag;
        v_com /= m_ag;
        r_com /= m_ag;

        Vector L(0._f);
        SymmetricTensor I = SymmetricTensor::null(); // inertia tensor
        for (Size i : idxs) {
            const Vector dr = r[i] - r_com;
            L += m[i] * cross(dr, v[i] - v_com);
            I += m[i] * (SymmetricTensor::identity() * getSqrLength(dr) - outer(dr, dr));
        }

        Integrals value;
        value.omega = I.inverse() * L;
        return value;
    }
};

/// \brief Holds a set of aggregates.
///
/// Bound to \ref Storage object. Thread-safe??
class AggregateHolder : public IAggregateObserver, public Noncopyable {
private:
    /// Holds all aggregates for the storage given in constructor.
    Array<Aggregate> aggregates;

    /// Contains a pointer to aggregate containing the particle with given index.
    ///
    /// The array is precomputed in constructor and maintained during aggregate merging, it is necessary for
    /// fast particle-to-aggregate queries.
    Array<RawPtr<Aggregate>> particleToAggregate;

    /// Lock needed for thread-safe access to aggregates via \ref IAggregateObserver interface.
    mutable std::mutex mutex;

public:
    explicit AggregateHolder(Storage& storage) {
        // create an aggregate for each particle
        const Size n = storage.getParticleCnt();
        aggregates.reserve(n); // so that it doesn't reallocate and invalidate pointers
        for (Size i = 0; i < n; ++i) {
            Aggregate& ag = aggregates.emplaceBack(storage, i);
            particleToAggregate.emplaceBack(addressOf(ag));
        }
    }

    /// \brief Returns the aggregate holding the given particle.
    Aggregate& getAggregate(const Size particleIdx) {
        Aggregate& ag = *particleToAggregate[particleIdx];
        ASSERT(ag.contains(particleIdx));
        return ag;
    }

    /// \copydoc getAggregate
    const Aggregate& getAggregate(const Size particleIdx) const {
        const Aggregate& ag = *particleToAggregate[particleIdx];
        ASSERT(ag.contains(particleIdx));
        return ag;
    }

    /// \brief Merges two aggregates.
    void merge(Aggregate& ag1, Aggregate& ag2) {
        CHECK_FUNCTION(CheckFunction::NON_REENRANT);
        std::unique_lock<std::mutex> lock(mutex);

        // add particles of ag2 to ag1
        ag1.merge(ag2);
        ASSERT(ag1.contains(ag1.getId()));
        // relink pointers of ag2 particles to ag1
        for (Size i : ag2) {
            particleToAggregate[i] = addressOf(ag1);
        }

        // remove all particles from ag2; we actually keep the aggregate in the holder (with zero particles)
        // for simplicity, otherwise we would have to shift the aggregates in the array, invalidating the
        // pointers held in particleToAggregate.
        ag2.clear();
        ASSERT(ag2.size() == 0);
    }

    /// \brief Integrates all aggregates
    void integrate() {
        // std::unique_lock<std::mutex> lock(mutex);
        for (Aggregate& ag : aggregates) {
            ag.integrate();
        }
    }

    /// \brief Locks the object using an upgradeable lock. Has to be called
    /*UpgradeableLock lock() {
        return UpgradeableLock(mutex);
    }*/

    virtual Optional<Size> getAggregateId(const Size particleIdx) const override {
        std::unique_lock<std::mutex> lock(mutex);
        const Aggregate& ag = getAggregate(particleIdx);
        if (ag.size() > 1) {
            return ag.getId();
        } else {
            return NOTHING;
        }
    }

    virtual Size count() const override {
        std::unique_lock<std::mutex> lock(mutex);
        Size cnt = 0;
        for (const Aggregate& ag : aggregates) {
            if (ag.size() > 1) {
                cnt++;
            }
        }
        return cnt;
    }
};

class AggregateCollisionHandler : public ICollisionHandler {
private:
    ArrayView<const Vector> r;
    ArrayView<const Vector> v;
    ArrayView<const Float> m;
    Float mergingLimit;

    struct {
        Float n;
        Float t;
    } restitution;

    RawPtr<AggregateHolder> holder;

public:
    explicit AggregateCollisionHandler(const RunSettings& settings) {
        mergingLimit = settings.get<Float>(RunSettingsId::COLLISION_MERGING_LIMIT);
        restitution.n = settings.get<Float>(RunSettingsId::COLLISION_RESTITUTION_NORMAL);
        restitution.t = settings.get<Float>(RunSettingsId::COLLISION_RESTITUTION_TANGENT);
    }

    virtual void initialize(Storage& storage) override {
        holder = dynamicCast<AggregateHolder>(storage.getUserData().get());
        ASSERT(holder);

        r = storage.getValue<Vector>(QuantityId::POSITION);
        v = storage.getDt<Vector>(QuantityId::POSITION);
        m = storage.getValue<Float>(QuantityId::MASS);
    }

    virtual CollisionResult collide(const Size i, const Size j, FlatSet<Size>& UNUSED(toRemove)) override {
        // this function SHOULD be called by one thread only, so we do not need to lock here
        CHECK_FUNCTION(CheckFunction::NON_REENRANT);

        Aggregate& ag_i = holder->getAggregate(i);
        Aggregate& ag_j = holder->getAggregate(j);
        if (addressOf(ag_i) == addressOf(ag_j)) {
            // particles belong to the same aggregate, do not process collision
            return CollisionResult::NONE;
        }

        // if the particles are gravitationally bound, add them to the aggregate, otherwise bounce
        if (areParticlesBound(m[i] + m[j], r[i][H] + r[j][H], v[i] - v[j], mergingLimit)) {
            // add to aggregate
            holder->merge(ag_i, ag_j);
            ag_i.integrate();
            return CollisionResult::NONE;
        } else {
            const Float m_i = ag_i.mass();
            const Vector v_i = ag_i.velocity();
            const Float m_j = ag_j.mass();
            const Vector v_j = ag_j.velocity();

            const Vector dr = getNormalized(r[i] - r[j]);
            const Vector v_com = weightedAverage(v_i, m_i, v_j, m_j);
            ag_i.setVelocity(this->reflect(v_i, v_com, -dr));
            ag_j.setVelocity(this->reflect(v_j, v_com, dr));

            return CollisionResult::BOUNCE;
        }
    }

private:
    /// \todo copied from bounce handler, deduplicate!!
    INLINE Vector reflect(const Vector& v, const Vector& v_com, const Vector& dir) {
        ASSERT(almostEqual(getSqrLength(dir), 1._f), dir);
        const Vector v_rel = v - v_com;
        const Float proj = dot(v_rel, dir);
        const Vector v_t = v_rel - proj * dir;
        const Vector v_n = proj * dir;

        // flip the orientation of normal component (bounce) and apply coefficients of restitution
        return restitution.t * v_t - restitution.n * v_n + v_com;
    }
};

class AggregateOverlapHandler : public IOverlapHandler {
private:
    RawPtr<AggregateHolder> holder;

public:
    virtual void initialize(Storage& storage) override {
        holder = dynamicCast<AggregateHolder>(storage.getUserData().get());
        ASSERT(holder);
    }

    virtual bool overlaps(const Size i, const Size j) const override {
        // this is called from multiple threads, but we are not doing any merging here
        const Aggregate& ag_i = holder->getAggregate(i);
        const Aggregate& ag_j = holder->getAggregate(j);
        if (addressOf(ag_i) == addressOf(ag_j)) {
            // false as in "overlap does not have to be handled"
            return false;
        }
        return true;
    }

    virtual void handle(const Size i, const Size j, FlatSet<Size>& UNUSED(toRemove)) override {
        // this function SHOULD be called by one thread only, so we do not need to lock here
        CHECK_FUNCTION(CheckFunction::NON_REENRANT);

        Aggregate& ag_i = holder->getAggregate(i);
        Aggregate& ag_j = holder->getAggregate(j);

        // even though we previously checked for this in function overlaps, the particles might have been
        // assinged to the same aggregate during collision processing, so we have to check again
        if (addressOf(ag_i) != addressOf(ag_j)) {
            holder->merge(ag_i, ag_j);
            ag_i.integrate();
        }
    }
};

AggregateSolver::AggregateSolver(IScheduler& scheduler, const RunSettings& settings)
    : NBodySolver(scheduler,
          settings,
          Factory::getGravity(settings),
          makeAuto<AggregateCollisionHandler>(settings),
          makeAuto<AggregateOverlapHandler>()) {}

//  //RepelHandler<ElasticBounceHandler>>(settings)) {}

AggregateSolver::~AggregateSolver() = default;

void AggregateSolver::integrate(Storage& storage, Statistics& stats) {
    createLazy(storage);
    NBodySolver::integrate(storage, stats);
    holder->integrate();

    stats.set(StatisticsId::AGGREGATE_COUNT, int(holder->count()));
}

void AggregateSolver::collide(Storage& storage, Statistics& stats, const Float dt) {
    createLazy(storage);
    NBodySolver::collide(storage, stats, dt);
    holder->integrate();
}

void AggregateSolver::createLazy(Storage& storage) {
    if (storage.getUserData()) {
        // already initialized
        return;
    }

    holder = makeShared<AggregateHolder>(storage);
    storage.setUserData(holder);
}

NAMESPACE_SPH_END
