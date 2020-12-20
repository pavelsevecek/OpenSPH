#include "gravity/AggregateSolver.h"
#include "gravity/Collision.h"
#include "gravity/IGravity.h"
#include "io/Logger.h"
#include "post/Analysis.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "sph/Materials.h"
#include "system/Factory.h"
#include "system/Statistics.h"
#include "thread/CheckFunction.h"
#include <mutex>

NAMESPACE_SPH_BEGIN

const Vector MAX_SPIN = Vector(0.1_f);

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

    /// Single particle
    Aggregate(Storage& storage, const Size particleIdx)
        : storage(addressOf(storage)) {
        idxs.insert(particleIdx);
        persistentId = particleIdx;
    }

    explicit Aggregate(Storage& storage, IndexSequence seq)
        : storage(addressOf(storage)) {
        for (Size i : seq) {
            idxs.insert(i);
        }
        persistentId = *seq.begin();
    }

    void add(const Size idx) {
        ASSERT(!idxs.contains(idx));
        idxs.insert(idx);
        //        this->fixVelocities();
    }

    void remove(const Size idx) {
        idxs.erase(idxs.find(idx));
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

    /// Modifies velocities according to saved angular frequency
    void spin() {
        if (this->size() == 1) {
            return;
        }

        ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITION);
        ArrayView<Vector> v = storage->getDt<Vector>(QuantityId::POSITION);
        ArrayView<Vector> alpha = storage->getValue<Vector>(QuantityId::PHASE_ANGLE);
        ArrayView<const Vector> w = storage->getValue<Vector>(QuantityId::ANGULAR_FREQUENCY);

        ArrayView<const Float> m = storage->getValue<Float>(QuantityId::MASS);
        Float m_ag = 0._f;
        Vector r_com(0._f);
        Vector v_com(0._f);
        for (Size i : idxs) {
            r_com += m[i] * r[i];
            v_com += m[i] * v[i];
            m_ag += m[i];
        }
        r_com /= m_ag;
        v_com /= m_ag;
        ASSERT(isReal(r_com) && getLength(r_com) < LARGE, r_com);

        const Vector omega = clamp(w[persistentId], -MAX_SPIN, MAX_SPIN);
        AffineMatrix rotationMatrix = AffineMatrix::identity();

        if (alpha[persistentId] != Vector(0._f)) {
            Vector dir;
            Float angle;
            tieToTuple(dir, angle) = getNormalizedWithLength(alpha[persistentId]);
            alpha[persistentId] = Vector(0._f);
            rotationMatrix = AffineMatrix::rotateAxis(dir, angle);
        }

        for (Size i : idxs) {
            ASSERT(alpha[i] == Vector(0._f));
            Float h = r[i][H];
            r[i] = r_com + rotationMatrix * (r[i] - r_com);
            v[i] += cross(omega, r[i] - r_com);
            r[i][H] = h;
            v[i][H] = 0._f;
        }
    }

    /// Saves angular frequency, sets velocities to COM movement
    void integrate() {
        ASSERT(this->size() > 0);
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
            I += m[i] * (SymmetricTensor::identity() * getSqrLength(dr) - symmetricOuter(dr, dr));
            M += m[i] * cross(dr, dv[i] - dv_com);
        }

        ArrayView<Vector> w, dw;
        tie(w, dw) = storage->getAll<Vector>(QuantityId::ANGULAR_FREQUENCY);


        const bool singular = I.determinant() == 0._f;
        Vector omega = Vector(0._f);
        if (!singular) {

            omega = I.inverse() * L;

            // const Vector w_loc = Em.transpose() * w[index];

            // compute the change of angular velocity using Euler's equations
            // dw[index] = I_inv * (M - cross(w[index], I * w[index]));
        }


        /*const SymmetricTensor I = Post::getInertiaTensor(m, r, r_com);
        (void)I;*/

        ArrayView<Vector> alpha, dalpha;
        tie(alpha, dalpha) = storage->getAll<Vector>(QuantityId::PHASE_ANGLE);

        for (Size i : idxs) {
            v[i] = v_com;
            dv[i] = dv_com;
            w[i] = omega;

            ASSERT(alpha[i] == Vector(0._f));
        }
        alpha[persistentId] = Vector(0._f);
        dalpha[persistentId] = w[persistentId];
    }

    // replaces unordered motion with bulk velocity + rotation
    void fixVelocities() {
        const Integrals ag = this->getIntegrals();
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
        for (Size i : idxs) {
            v[i] = ag.v_com + cross(ag.omega, r[i] - ag.r_com);
            v[i][H] = 0._f;
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

    void displace(const Vector& offset) {
        ASSERT(offset[H] == 0._f);

        ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITION);
        for (Size i : idxs) {
            r[i] += offset;
        }
    }


    Size size() const {
        return idxs.size();
    }

    bool empty() const {
        return idxs.empty();
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
        Vector L;
    };

    Integrals getIntegrals() const {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m = storage->getValue<Float>(QuantityId::MASS);

        Float m_ag = 0._f;
        Vector r_com(0._f);
        Vector v_com(0._f);
        for (Size i : idxs) {
            v_com += m[i] * v[i];
            r_com += m[i] * r[i];
            m_ag += m[i];
        }
        v_com /= m_ag;
        r_com /= m_ag;

        Vector L(0._f);
        SymmetricTensor I = SymmetricTensor::null(); // inertia tensor
        for (Size i : idxs) {
            const Vector dr = r[i] - r_com;
            L += m[i] * cross(dr, v[i] - v_com);
            I += m[i] * (SymmetricTensor::identity() * getSqrLength(dr) - symmetricOuter(dr, dr));
        }

        Integrals value;
        value.m = m_ag;
        value.r_com = r_com;
        value.v_com = v_com;
        value.I = I;
        value.L = L;

        if (I.determinant() != 0._f) {
            value.omega = clamp(I.inverse() * L, -MAX_SPIN, MAX_SPIN);
        } else {
            value.omega = Vector(0._f);
        }
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
    AggregateHolder(Storage& storage, const AggregateEnum source) {
        // create an aggregate for each particle, even if its empty, so we can add particles to it later
        // without reallocations
        const Size n = storage.getParticleCnt();
        aggregates.resize(n);

        switch (source) {
        case AggregateEnum::PARTICLES: {
            for (Size i = 0; i < n; ++i) {
                aggregates[i] = Aggregate(storage, i);
                particleToAggregate.emplaceBack(addressOf(aggregates[i]));
            }
            break;
        }
        case AggregateEnum::MATERIALS: {
            for (Size matId = 0; matId < storage.getMaterialCnt(); ++matId) {
                MaterialView mat = storage.getMaterial(matId);
                IndexSequence seq = mat.sequence();
                // need to create all aggregates to storage pointer to storage
                for (Size i : seq) {
                    aggregates[i] = Aggregate(storage, i);
                    aggregates[i].clear();
                }

                const Size idx = *seq.begin();
                aggregates[idx] = Aggregate(storage, seq);

                for (Size i = 0; i < seq.size(); ++i) {
                    particleToAggregate.emplaceBack(addressOf(aggregates[idx]));
                }
            }
            break;
        }
        default:
            NOT_IMPLEMENTED;
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
    /*merge(Aggregate& ag1, Aggregate& ag2) {
         CHECK_FUNCTION(CheckFunction::NON_REENRANT);
         std::unique_lock<std::mutex> lock(mutex);

         if (ag1.size() > ag1.size()) {
             ag1.merge(ag2);
         } else {
             ag2.merge(ag1);
         }
         // ASSERT(ag1.contains(ag1.getId()));
         // relink pointers of ag2 particles to ag1
         for (Size i : ag2) {
             particleToAggregate[i] = addressOf(ag1);
         }

         // remove all particles from ag2; we actually keep the aggregate in the holder (with zero particles)
         // for simplicity, otherwise we would have to shift the aggregates in the array, invalidating the
         // pointers held in particleToAggregate.
         ag2.clear();
         ASSERT(ag2.size() == 0);
     }*/

    void merge(Aggregate& ag1, Aggregate& ag2) {
        if (ag1.size() < ag2.size()) {
            this->merge(ag2, ag1);
            return;
        }

        ASSERT(ag1.size() >= ag2.size());
        // accumulate single particle
        if (ag2.size() == 1) {
            const Size id = ag2.getId();
            ag1.add(id);
            particleToAggregate[id] = &ag1;
            ag2.clear();
        } else {

            // break the aggregate
            this->disband(ag2);
        }

        ag1.fixVelocities();
    }

    void separate(Aggregate& ag, const Size idx) {

        if (ag.getId() == idx) {
            return; /// \todo ?? how to do this
        }

        aggregates[idx].add(idx);
        particleToAggregate[idx] = addressOf(aggregates[idx]);
        ag.remove(idx);
        ag.fixVelocities();
    }

    void disband(Aggregate& ag) {
        const Size mainId = ag.getId();
        for (Size i : ag) {
            // put the particle back to its original aggregate
            if (i != mainId) {
                aggregates[i].add(i);
                particleToAggregate[i] = addressOf(aggregates[i]);
            }
        }

        ag.clear();
        ag.add(mainId);
    }

    void spin() {
        // std::unique_lock<std::mutex> lock(mutex);
        for (Aggregate& ag : aggregates) {
            if (!ag.empty()) {
                ag.spin();
            }
        }
    }

    /// \brief Integrates all aggregates
    void integrate() {
        // std::unique_lock<std::mutex> lock(mutex);

        for (Aggregate& ag : aggregates) {
            if (!ag.empty()) {
                ag.integrate();
            }
        }
    }

    Optional<Size> getAggregateId(const Size particleIdx) const {
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
    Float bounceLimit;

    struct {
        Float n;
        Float t;
    } restitution;

    RawPtr<AggregateHolder> holder;
    ArrayView<const Vector> r;
    ArrayView<Vector> v;
    ArrayView<const Float> m;

public:
    explicit AggregateCollisionHandler(const RunSettings& settings) {
        bounceLimit = settings.get<Float>(RunSettingsId::COLLISION_BOUNCE_MERGE_LIMIT);
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

        const Vector v_com = weightedAverage(v[i], m[i], v[j], m[j]);
        const Vector dr = getNormalized(r[i] - r[j]);
        v[i] = this->reflect(v[i], v_com, -dr);
        v[j] = this->reflect(v[j], v_com, dr);
        v[i][H] = v[j][H] = 0._f;

        /*if (getSqrLength(v[i] - ag_i.velocity()) > sqr(MAX_STRAIN)) {
            holder->separate(ag_i, i);
        }

        if (getSqrLength(v[j] - ag_j.velocity()) > sqr(MAX_STRAIN)) {
            holder->separate(ag_j, j);
        }*/

        // particle are moved back after collision handling, so we need to make sure they have correct
        // velocities to not move them away from the aggregate
        ag_i.fixVelocities();
        ag_j.fixVelocities();

        /*
                if (ag_i.size() > ag_j.size()) {
                    holder->disband(ag_j);
                } else {
                    holder->disband(ag_i);
                }*/

        // if the particles are gravitationally bound, add them to the aggregate, otherwise bounce
        if (areParticlesBound(m[i] + m[j], r[i][H] + r[j][H], v[i] - v[j], bounceLimit)) {
            // add to aggregate
            holder->merge(ag_i, ag_j);

            return CollisionResult::NONE;


        } else {
            holder->separate(ag_i, i);
            holder->separate(ag_j, j);

            return CollisionResult::BOUNCE;
        }
    }

private:
    /// \todo copied from bounce handler, deduplicate!!
    INLINE Vector reflect(const Vector& v, const Vector& v_com, const Vector& dir) const {
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
    ArrayView<const Float> m;
    ArrayView<Vector> r;

    AggregateCollisionHandler handler;

public:
    explicit AggregateOverlapHandler(const RunSettings& settings)
        : handler(settings) {}

    virtual void initialize(Storage& storage) override {
        holder = dynamicCast<AggregateHolder>(storage.getUserData().get());
        ASSERT(holder);

        handler.initialize(storage);
        m = storage.getValue<Float>(QuantityId::MASS);
        r = storage.getValue<Vector>(QuantityId::POSITION);
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

    virtual void handle(const Size i, const Size j, FlatSet<Size>& toRemove) override {
        // this function SHOULD be called by one thread only, so we do not need to lock here
        CHECK_FUNCTION(CheckFunction::NON_REENRANT);

        Aggregate& ag_i = holder->getAggregate(i);
        Aggregate& ag_j = holder->getAggregate(j);

        // even though we previously checked for this in function overlaps, the particles might have been
        // assinged to the same aggregate during collision processing, so we have to check again
        if (addressOf(ag_i) == addressOf(ag_j)) {
            return;
        }

        Vector dir;
        Float dist;
        tieToTuple(dir, dist) = getNormalizedWithLength(r[i] - r[j]);
        dir[H] = 0._f; // don't mess up radii

        if (dist > r[i][H] + r[j][H]) {
            // not a real overlap
            return;
        }

        const Float m1 = ag_i.mass(); /// \todo precompute
        const Float m2 = ag_j.mass();
        const Float x1 = (r[i][H] + r[j][H] - dist) / (1._f + m1 / m2);
        const Float x2 = m1 / m2 * x1;
        ag_i.displace(dir * x1);
        ag_j.displace(-dir * x2);

        handler.collide(i, j, toRemove);
    }
};

AggregateSolver::AggregateSolver(IScheduler& scheduler, const RunSettings& settings)
    : HardSphereSolver(scheduler,
          settings,
          Factory::getGravity(settings),
          makeAuto<AggregateCollisionHandler>(settings),
          makeAuto<AggregateOverlapHandler>(settings)) {}
// makeAuto<RepelHandler<AggregateCollisionHandler>>(settings)) {}


AggregateSolver::~AggregateSolver() = default;

void AggregateSolver::integrate(Storage& storage, Statistics& stats) {
    holder->spin();
    HardSphereSolver::integrate(storage, stats);
    holder->integrate();

    // storage IDs and aggregate stats
    ArrayView<Size> aggregateIds = storage.getValue<Size>(QuantityId::AGGREGATE_ID);
    for (Size i = 0; i < aggregateIds.size(); ++i) {
        aggregateIds[i] = holder->getAggregateId(i).valueOr(Size(-1));
    }
    stats.set(StatisticsId::AGGREGATE_COUNT, int(holder->count()));
}

void AggregateSolver::collide(Storage& storage, Statistics& stats, const Float dt) {
    holder->spin();
    HardSphereSolver::collide(storage, stats, dt);
    holder->integrate();
}

void AggregateSolver::create(Storage& storage, IMaterial& material) const {
    HardSphereSolver::create(storage, material);

    storage.insert<Vector>(QuantityId::ANGULAR_FREQUENCY, OrderEnum::FIRST, Vector(0._f));
    storage.insert<Vector>(QuantityId::PHASE_ANGLE, OrderEnum::FIRST, Vector(0._f));
    storage.insert<Size>(QuantityId::AGGREGATE_ID, OrderEnum::ZERO, Size(-1));
}

void AggregateSolver::createAggregateData(Storage& storage, const AggregateEnum source) {
    holder = makeShared<AggregateHolder>(storage, source);
    storage.setUserData(holder);
}

NAMESPACE_SPH_END
