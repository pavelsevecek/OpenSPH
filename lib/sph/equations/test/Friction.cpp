#include "sph/equations/Friction.h"
#include "catch.hpp"
#include "objects/geometry/Domain.h"
#include "sph/initial/Initial.h"
#include "sph/solvers/SymmetricSolver.h"
#include "system/Statistics.h"
#include "tests/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;

TEST_CASE("InternalFriction", "[friction]") {
    EquationHolder eqs;
    RunSettings settings;
    settings.set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, false);
    eqs += makeTerm<InternalFriction>() + makeTerm<BenzAsphaugSph::ContinuityEquation>() +
           makeTerm<ConstSmoothingLength>();
    SymmetricSolver solver(settings, std::move(eqs));

    Storage storage;
    InitialConditions initial(solver, RunSettings::getDefaults());
    BodySettings body;
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE);
    body.set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE);
    body.set(BodySettingsId::EOS, EosEnum::NONE);
    body.set(BodySettingsId::PARTICLE_COUNT, 10000);
    initial.addMonolithicBody(storage, BlockDomain(Vector(0._f), Vector(2._f, 2._f, 1._f)), body);

    // add two sliding layers
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        if (r[i][Z] > 0._f) {
            v[i] = Vector(10._f, 0._f, 0._f);
        }
    }
    Statistics stats;
    solver.integrate(storage, stats);
    ArrayView<const Size> neighs = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    const Float h = r[0][H];
    auto test = [&](const Size i) -> Outcome {
        if (max(abs(r[i][X]), abs(r[i][Y]), abs(r[i][Z])) > 0.8_f) {
            // skip boundary particles
            return SUCCESS;
        }
        if (Interval(0._f, h).contains(r[i][Z])) {
            // these particles should be slowed down
            if (dv[i][X] >= -1.e-5_f) {
                return makeFailed("Friction didn't decelerate:\n",
                    dv[i],
                    "\nr =",
                    r[i],
                    ", v = ",
                    v[i],
                    "\n neigh cnt = ",
                    neighs[i]);
            }
            return SUCCESS;
        }
        if (Interval(h, 1._f).contains(r[i][Z])) {
            // these should either be slowed down or remain unaffected
            if (dv[i][X] > 0._f) {
                return makeFailed("Friction accelerated where is shouldn't\n", dv[i]);
            }
            return SUCCESS;
        }
        if (Interval(-h, 0._f).contains(r[i][Z])) {
            // these particles should be accelerated in X
            if (dv[i][X] <= 1.e-5_f) {
                return makeFailed("Friction didn't accelerate:\n",
                    dv[i],
                    "\nr = ",
                    r[i],
                    ", v = ",
                    v[i],
                    "\n neigh cnt = ",
                    neighs[i]);
            }
            return SUCCESS;
        }
        if (Interval(-1._f, -h).contains(r[i][Z])) {
            // these should either be accelerated or remain uaffected
            if (dv[i][X] < 0._f) {
                return makeFailed(
                    "Friction decelerated where is shouldn't\n", dv[i], "\nr = ", r[i], ", v = ", v[i]);
            }
            return SUCCESS;
        }
        STOP; // sanity check that we checked all particles
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}
