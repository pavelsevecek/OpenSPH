#include "gravity/CachedGravity.h"
#include "catch.hpp"
#include "tests/Approx.h"
#include "tests/Setup.h"
#include "utils/SequenceTest.h"

using namespace Sph;

struct TestGravity : public IGravity {
    virtual void build(IScheduler& UNUSED(scheduler), const Storage& UNUSED(storage)) override {}

    virtual void evalSelfGravity(IScheduler& UNUSED(scheduler),
        ArrayView<Vector> dv,
        Statistics& stats) const override {
        for (Size i = 0; i < dv.size(); ++i) {
            if (stats.get<Float>(StatisticsId::RUN_TIME) < 5._f) {
                dv[i] += Vector(1._f, 0._f, 0._f);
            } else {
                dv[i] += Vector(0._f, 0._f, 1._f);
            }
        }
    }

    virtual void evalExternal(IScheduler& UNUSED(scheduler),
        ArrayView<Attractor> UNUSED(ps),
        ArrayView<Vector> UNUSED(dv)) const override {
        NOT_IMPLEMENTED;
    }

    virtual Vector evalAcceleration(const Vector& UNUSED(r0)) const override {
        NOT_IMPLEMENTED;
    }

    virtual Float evalEnergy(IScheduler& UNUSED(scheduler), Statistics& UNUSED(stats)) const override {
        NOT_IMPLEMENTED;
    }

    virtual RawPtr<const IBasicFinder> getFinder() const override {
        NOT_IMPLEMENTED;
    }
};

TEST_CASE("CachedGravity add acceleration", "[gravity]") {
    CachedGravity cached(2._f, makeAuto<TestGravity>());

    Array<Vector> dv(5);
    dv.fill(Vector(0._f, 2._f, 0._f));
    Statistics stats;

    stats.set(StatisticsId::RUN_TIME, 1._f);
    cached.evalSelfGravity(SEQUENTIAL, dv, stats);
    REQUIRE(std::all_of(dv.begin(), dv.end(), [](Vector& a) { return a == Vector(1._f, 2._f, 0._f); }));

    stats.set(StatisticsId::RUN_TIME, 2._f); // after 1s, should use the cached accelerations
    cached.evalSelfGravity(SEQUENTIAL, dv, stats);
    REQUIRE(std::all_of(dv.begin(), dv.end(), [](Vector& a) { return a == Vector(2._f, 2._f, 0._f); }));

    stats.set(StatisticsId::RUN_TIME, 6._f); // should recompute and use the dv after 5s
    cached.evalSelfGravity(SEQUENTIAL, dv, stats);
    REQUIRE(std::all_of(dv.begin(), dv.end(), [](Vector& a) { return a == Vector(2._f, 2._f, 1._f); }));
}
