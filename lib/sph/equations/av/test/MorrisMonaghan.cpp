#include "sph/equations/av/MorrisMonaghan.h"
#include "catch.hpp"
#include "sph/solvers/AsymmetricSolver.h"
#include "sph/solvers/SymmetricSolver.h"
#include "tests/Approx.h"
#include "tests/Setup.h"
#include "utils/SequenceTest.h"
#include "utils/Utils.h"

using namespace Sph;

TYPED_TEST_CASE_2("MorrisMonaghan sanitycheck", "[av]", TSolver, SymmetricSolver, AsymmetricSolver) {
    BodySettings body;
    body.set(BodySettingsId::DENSITY, 1._f).set(BodySettingsId::ENERGY, 1._f);
    Storage storage = Tests::getGassStorage(1000, body);
    const Float cs = storage.getValue<Float>(QuantityId::SOUND_SPEED)[0];

    Tests::computeField<TSolver>(storage, makeTerm<MorrisMonaghanAV>(), [cs](const Vector r) {
        // supersonic shock at x=0
        if (r[X] > 0.f) {
            return Vector(-25._f * cs, 0._f, 0._f);
        } else {
            return Vector(0._f, 0._f, 0._f);
        }
    });

    // check that AV increases in the shock and decays far away from it
    ArrayView<Float> dalpha = storage.getDt<Float>(QuantityId::AV_ALPHA);
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    const Float h = r[0][H];

    auto test = [&](const Size i) -> Outcome {
        if (abs(r[i][X]) < h && dalpha[i] < 0.1_f) {
            return makeFailed("AV didn't increase inside the shock: \n d_alpha = ", dalpha[i]);
        }
        if (abs(r[i][X]) > 2._f * h && dalpha[i] > -0.1_f) {
            return makeFailed("AV didn't decrease far away from the shock: \n d_alpha = ", dalpha[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}
