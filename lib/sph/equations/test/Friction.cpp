#include "sph/equations/Friction.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "sph/initial/Initial.h"
#include "sph/solvers/GenericSolver.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;

TEST_CASE("ImternalFriction", "[friction]") {
    EquationHolder eqs;
    eqs += makeTerm<InternalFriction>() + makeTerm<ContinuityEquation<DensityEvolution::FLUID>>();
    GenericSolver solver(RunSettings::getDefaults(), std::move(eqs));

    Storage storage;
    InitialConditions initial(storage, solver, RunSettings::getDefaults());
    BodySettings body;
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE);
    body.set(BodySettingsId::RHEOLOGY_DAMAGE, DamageEnum::NONE);
    body.set(BodySettingsId::EOS, EosEnum::NONE);
    body.set(BodySettingsId::PARTICLE_COUNT, 10000);
    initial.addBody(BlockDomain(Vector(0._f), Vector(2._f, 2._f, 1._f)), body);

    // add two sliding layers
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
    for (Size i = 0; i < r.size(); ++i) {
        if (r[i][Z] > 0._f) {
            v[i] = Vector(10._f, 0._f, 0._f);
        }
    }
    Statistics stats;
    solver.integrate(storage, stats);
    ArrayView<const Size> neighs = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
    const Float h = r[0][H];
    auto test = [&](const Size i) -> Outcome {
        if (max(abs(r[i][X]), abs(r[i][Y]), abs(r[i][Z])) > 0.8_f) {
            // skip boundary particles
            return SUCCESS;
        }
        if (Range(0._f, h).contains(r[i][Z])) {
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
        if (Range(h, 1._f).contains(r[i][Z])) {
            // these should either be slowed down or remain unaffected
            if (dv[i][X] > 0._f) {
                return makeFailed("Friction accelerated where is shouldn't\n", dv[i]);
            }
            return SUCCESS;
        }
        if (Range(-h, 0._f).contains(r[i][Z])) {
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
        if (Range(-1._f, -h).contains(r[i][Z])) {
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