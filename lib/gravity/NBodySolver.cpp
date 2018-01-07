#include "gravity/NBodySolver.h"

NAMESPACE_SPH_BEGIN

NBodySolver::NBodySolver(const RunSettings& settings)
    : NBodySolver(settings, makeAuto<BruteForceGravity>()) { // Factory::getGravity(settings)) {
}

NBodySolver::NBodySolver(const RunSettings& settings, AutoPtr<IGravity>&& gravity)
    : gravity(std::move(gravity))
    , pool(settings.get<int>(RunSettingsId::RUN_THREAD_CNT))
    , collisionFinder(Factory::getFinder(settings))
    , collisionHandler(Factory::getCollisionHandler(settings)) {
    /// \todo check if this parameter is still needed
    allowedOverlap = settings.get<Float>(RunSettingsId::COLLISION_ALLOWED_OVERLAP);
}

static void integrateRigidBody(Storage& storage) {
    ArrayView<SymmetricTensor> I, dI;
    tie(I, dI) = storage.getAll<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA);
    ArrayView<Vector> w, dw;
    tie(w, dw) = storage.getAll<Vector>(QuantityId::ANGULAR_VELOCITY);

    for (Size i = 0; i < w.size(); ++i) {
        AffineMatrix w_cross = AffineMatrix::crossProductOperator(w[i]);
        // we need to convert to affine matrix, as the intermediate steps break the symmetry of the tensor
        AffineMatrix Im = convert(I[i]);
        AffineMatrix dIm = w_cross * Im - Im * w_cross;
        // convert back to the symmetric tensor, the result should be symmetric (checked by assert)
        dI[i] = convert(dIm);

        SymmetricTensor Iinv = I[i].inverse();

        // using torque-free solution
        // I dw/dt + dI/dt * w = 0
        dw[i] = -Iinv * (dI[i] * w[i]);
    }
}

void NBodySolver::integrate(Storage& storage, Statistics& stats) {
    gravity->build(storage);

    ArrayView<Vector> dv = storage.getD2t<Vector>(QuantityId::POSITION);
    gravity->evalAll(pool, dv, stats);

    // null all derivatives of smoothing lengths (particle radii)
    ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < v.size(); ++i) {
        v[i][H] = 0._f;
        dv[i][H] = 0._f;
    }

    // rigid body dynamics
    integrateRigidBody(storage);
}

void NBodySolver::collide(Storage& storage, Statistics& stats, const Float dt) {
    ArrayView<Vector> r, v, a;
    tie(r, v, a) = storage.getAll<Vector>(QuantityId::POSITION);

    // find the radius of possible colliders
    const Float searchRadius = this->getSearchRadius(r, v, dt);

    // tree for finding collisions
    collisionFinder->build(r);

    // handler determining collison outcomes
    collisionHandler->initialize(storage);

    Size collisionCounter = 0;
    Array<Size> toRemove;

    // holds computed collisions
    std::set<CollisionRecord> collisions;

    // holds indices of particles collisions of which have been invalidated and needs to be recomputed
    std::set<Size> invalidIdx;

    // first pass - find all collisions and sort them by collision time
    for (Size i = 0; i < r.size(); ++i) {
        if (CollisionRecord col = this->findClosestCollision(i, searchRadius, dt, neighs, r, v)) {
            collisions.insert(col);
        }
    }

    Float time = 0._f; // dt;
    while (!collisions.empty()) {
        const CollisionRecord& col = *collisions.begin();
        const Float t_coll = col.t_coll;
        ASSERT(t_coll < dt);

        Size i = col.i;
        Size j = col.j;

        // advance all positions to the collision time
        if (t_coll != time) {
            for (Size i = 0; i < r.size(); ++i) {
                r[i] += v[i] * (t_coll - time);
            }
        }

        // handle the collision
        toRemove.clear();
        const CollisionResult result = collisionHandler->collide(i, j, toRemove);
        if (result != CollisionResult::NONE) {
            ASSERT(storage.isValid());
            time = t_coll; // move the time to the current collision time

            // remove all collisions containing either i or j
            invalidIdx.clear();
            for (auto iter = collisions.begin(); iter != collisions.end();) {
                const CollisionRecord& c = *iter;
                if (c.i == i || c.i == j || c.j == i || c.j == j) {
                    if (std::find(toRemove.begin(), toRemove.end(), c.i) == toRemove.end()) {
                        // not removed, so we need to recompute the collisions
                        Size k = c.i;
                        for (Size idx : toRemove) {
                            if (idx < k) {
                                --k;
                            }
                        }
                        invalidIdx.insert(k);
                    }
                    /// \todo remove code dupl
                    if (std::find(toRemove.begin(), toRemove.end(), c.j) == toRemove.end()) {
                        Size k = c.j;
                        for (Size idx : toRemove) {
                            if (idx < k) {
                                --k;
                            }
                        }
                        invalidIdx.insert(k);
                    }
                    iter = collisions.erase(iter);
                } else {
                    ++iter;
                }
            }
            // modify all the collisions to keep valid indices
            for (Size idx : toRemove) {
                for (const CollisionRecord& c : collisions) {
                    ASSERT(idx != c.i && idx != c.j);
                    if (idx < c.i) {
                        --c.i;
                    }
                    if (idx < c.j) {
                        --c.j;
                    }
                }
            }

            // apply the removal list
            if (!toRemove.empty()) {
                storage.remove(toRemove);
                // remove it also from all dependent storages, since this is a permanent action
                storage.propagate([&toRemove](Storage& dependent) { dependent.remove(toRemove); });

                // reset the invalidated arrayviews and rebuild the finder
                tie(r, v, a) = storage.getAll<Vector>(QuantityId::POSITION);
                collisionFinder->build(r);
            }

            // find new collisions for invalid indices
            for (Size idx : invalidIdx) {
                // here we shouldn't find any removed particle
                // ASSERT(std::find(toRemove.begin(), toRemove.end(), idx) == toRemove.end());

                if (CollisionRecord c =
                        this->findClosestCollision(idx, searchRadius, dt - time, neighs, r, v)) {
                    // ASSERT(std::find(toRemove.begin(), toRemove.end(), c.i) == toRemove.end());
                    // ASSERT(std::find(toRemove.begin(), toRemove.end(), c.j) == toRemove.end());
                    if ((c.i == i && c.j == j) || (c.j == i && c.i == j)) {
                        // don't process the same pair twice in a row
                        continue;
                    }
                    c.t_coll += time;
                    collisions.insert(c);
                }
            }

            collisionCounter++;
        }
    }
    // advance all positions by the remaining time
    for (Size i = 0; i < r.size(); ++i) {
        r[i] += v[i] * (dt - time);
    }
    stats.set(StatisticsId::COLLISION_COUNT, int(collisionCounter));
}

void NBodySolver::create(Storage& storage, IMaterial& UNUSED(material)) const {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);

    storage.insert<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA, OrderEnum::FIRST, SymmetricTensor::null());
    ArrayView<SymmetricTensor> I = storage.getValue<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA);
    for (Size i = 0; i < r.size(); ++i) {
        I[i] = Rigid::sphereInertia(m[i], r[i][H]);
    }
    storage.insert<Vector>(QuantityId::ANGULAR_VELOCITY, OrderEnum::FIRST, Vector(0._f));
}

Float NBodySolver::getSearchRadius(ArrayView<const Vector> r, ArrayView<const Vector> v, const Float dt) {
    // find the largest velocity, so that we know how far to search for potentional impactors
    /// \todo naive implementation, improve
    Float v_max = 0._f;
    Float h_max = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        v_max = max(v_max, getSqrLength(v[i]));
        h_max = max(h_max, r[i][H]);
    }
    v_max = sqrt(v_max);
    return v_max * dt + h_max;
}

CollisionRecord NBodySolver::findClosestCollision(const Size i,
    const Float globalRadius,
    const Float dt,
    Array<NeighbourRecord>& neighs,
    ArrayView<Vector> r,
    ArrayView<Vector> v) const {
    // maximum travel of i-th particle
    const Float localRadius = r[i][H] + getLength(v[i]) * dt;
    // since we don't know a priori the travel distance of neighbours, use the maximum of all
    collisionFinder->findNeighbours(i, globalRadius + localRadius, neighs, EMPTY_FLAGS);

    CollisionRecord closestCollision;
    for (NeighbourRecord& n : neighs) {
        const Size j = n.index;
        if (j == i) {
            continue;
        }
        ASSERT(getLength(r[i] - r[j]) >= 0.9_f * (r[i][H] + r[j][H]),
            i,
            j,
            getLength(r[i] - r[j]),
            r[i][H] + r[j][H]);

        Optional<Float> t_coll = this->checkCollision(r[i], v[i], r[j], v[j], dt);
        if (t_coll) {
            ASSERT(t_coll.value() < dt && t_coll.value() < dt);
            closestCollision = min(closestCollision, CollisionRecord{ i, j, t_coll.value() });
        }
    }
    return closestCollision;
}

Optional<Float> NBodySolver::checkCollision(const Vector& r1,
    const Vector& v1,
    const Vector& r2,
    const Vector& v2,
    const Float dt) const {
    const Vector dr = r1 - r2;
    const Vector dv = v1 - v2;
    const Float dvdr = dot(dv, dr);
    if (dvdr > 0._f) {
        // not moving towards each other, no collision
        return NOTHING;
    }

    //        getLength(dv) < EPS * (getLength(v1) + getLength(v2)) ? getNormalized(dr) :
    //        getNormalized(dv);
    const Vector dr_perp = dr - dot(dv, dr) * dv / getSqrLength(dv);
    ASSERT(getSqrLength(dr_perp) < (1._f + EPS) * getSqrLength(dr), dr_perp, dr);
    if (getSqrLength(dr_perp) <= sqr(r1[H] + r2[H])) {
        // on collision trajectory, find the collision time
        const Float dv2 = getSqrLength(dv);
        const Float det = 1._f - (getSqrLength(dr) - sqr(r1[H] + r2[H])) / sqr(dvdr) * dv2;
        // ASSERT(det >= 0._f);
        const Float root = (det > 1._f + allowedOverlap) ? 1._f + sqrt(det) : 1._f - sqrt(max(0._f, det));
        const Float t_coll = -dvdr / dv2 * root;
        // collision time can be slightly negative number, corresponding to collision of overlaping
        // particles.
        /// \todo assert that t_coll is not VERY negative (but what very? It's sometimes 4x timestep, hm
        /// ...)
        ASSERT(isReal(t_coll));

        // collision happens if the collision time is less than the current timestep
        if (t_coll < dt) {
            return t_coll;
        }
    }
    return NOTHING;
}

NAMESPACE_SPH_END
