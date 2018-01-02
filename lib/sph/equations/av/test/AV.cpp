#include "catch.hpp"
#include "io/LogFile.h"
#include "objects/utility/PerElementWrapper.h"
#include "sph/equations/av/Riemann.h"
#include "sph/equations/av/Standard.h"
#include "tests/Approx.h"
#include "tests/Setup.h"
#include "utils/SequenceTest.h"
#include "utils/Utils.h"

using namespace Sph;

TYPED_TEST_CASE_2("AV divergent", "[av]", T, StandardAV, RiemannAV) {
    EquationHolder term = makeTerm<T>();
    Storage storage = Tests::getGassStorage(10000);
    Tests::computeField<SymmetricSolver>(storage, std::move(term), [](const Vector& r) {
        // some divergent velocity field
        return r;
    });
    ArrayView<Vector> dv = storage.getD2t<Vector>(QuantityId::POSITION);
    // AV shouldn't kick in for divergent flow
    REQUIRE(perElement(dv) == Vector(0._f));
}

TYPED_TEST_CASE_2("AV shockwave", "[av]", T, StandardAV, RiemannAV) {
    EquationHolder term = makeTerm<T>();
    BodySettings body;
    body.set(BodySettingsId::DENSITY, 1._f).set(BodySettingsId::ENERGY, 1._f);
    Storage storage = Tests::getGassStorage(10000, body);
    const Float cs = storage.getValue<Float>(QuantityId::SOUND_SPEED)[0]; // all particles have the same c_s
    REQUIRE(cs > 0._f);
    const Float v0 = 5._f * cs;
    Tests::computeField<SymmetricSolver>(storage, std::move(term), [v0](const Vector& r) {
        // zero velocity for x<0, supersonic flow for x>0
        if (r[X] < 0._f) {
            return Vector(0._f);
        } else {
            return Vector(-v0, 0._f, 0._f);
        }
    });
    // should add acceleration and heating to particles around x=0
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    ArrayView<Float> du = storage.getDt<Float>(QuantityId::ENERGY);
    const Float h = r[0][H];

    Size heatedCnt = 0;
    RunSettings settings;
    const Float alpha = settings.get<Float>(RunSettingsId::SPH_AV_ALPHA);
    const Float beta = settings.get<Float>(RunSettingsId::SPH_AV_BETA);
    auto test = [&](const Size i) -> Outcome {
        if (getLength(r[i]) > 0.7_f) {
            return SUCCESS; // skip boundary particles
        }
        if (abs(r[i][X]) < h) {
            /// \todo is this a correct estimate?
            const Float Pi = alpha * cs * v0 + beta * sqr(v0);
            const bool heated = getLength(dv[i]) >= Pi && du[i] > 0.5_f * Pi * v0;
            if (heated) {
                heatedCnt++;
                return SUCCESS;
            } else {
                return makeFailed(
                    "no acceleration or heating:\nr = ", r[i], "\ndv = ", dv[i], "\ndu = ", du[i]);
            }
        }
        if (abs(r[i][X]) > 2._f) {
            return makeOutcome(dv[i] == Vector(0._f) && du[i] == 0._f,
                "acceleration or heating in steady flow: ",
                dv[i],
                du[i]);
        }
        // particles close to abs(X)==2h may or may not interact with the flow, depends on the particles
        // distribution. Lets just return success here.
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
    REQUIRE(heatedCnt++); // we did test at least some heated particles
}

/// \todo more tests: somehow test that AV will stop particles for passing through each other
/// We could do this just for 2 particles: threw them against each other, AV should repel them (or just
/// slow down? the pressure term is the one doing the repelling in the end...)
