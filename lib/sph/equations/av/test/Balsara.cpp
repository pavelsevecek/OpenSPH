#include "sph/equations/av/Balsara.h"
#include "catch.hpp"
#include "sph/equations/av/Standard.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"
#include "utils/Setup.h"

using namespace Sph;

TEST_CASE("Balsara shear flow", "[av]") {

    // no switch
    EquationHolder term1 = makeTerm<StandardAV>();
    Storage storage1 = Tests::getGassStorage(10000);
    Tests::computeField(storage1, std::move(term1), [](const Vector& r) {
        // spin-up particles with some differential rotation
        const Vector l(r[X], r[Y], 0._f);
        return cross(Vector(0, 0, 1), l) / (getSqrLength(l) + 1._f);
    });

    // with switch
    Storage storage2 = Tests::getGassStorage(10000);
    EquationHolder term2 = makeTerm<BalsaraSwitch<StandardAV>>(RunSettings::getDefaults());
    // need to compute twice, first to get velocity divergence and rotation, second to compute AV
    Tests::computeField(storage2,
        std::move(term2),
        [](const Vector& r) {
            const Vector l(r[X], r[Y], 0._f);
            return cross(Vector(0, 0, 1), l) / (getSqrLength(l) + 1._f);
        },
        2);

    ArrayView<Vector> dv1 = storage1.getD2t<Vector>(QuantityId::POSITIONS);
    ArrayView<Float> du1 = storage1.getDt<Float>(QuantityId::ENERGY);
    ArrayView<Vector> dv2 = storage2.getD2t<Vector>(QuantityId::POSITIONS);
    ArrayView<Float> du2 = storage2.getDt<Float>(QuantityId::ENERGY);
    ArrayView<Float> divv = storage2.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
    ArrayView<Vector> rotv = storage2.getValue<Vector>(QuantityId::VELOCITY_ROTATION);
    ArrayView<Vector> r = storage2.getValue<Vector>(QuantityId::POSITIONS);

    auto test = [&](const Size i) {
        if (getLength(r[i]) >= 0.7_f) {
            // skip boundary particles
            return SUCCESS;
        }
        if (getLength(dv2[i]) > getLength(dv1[i]) || du2[i] > du1[i]) {
            return makeFailed("Balsara increased AV");
        }
        if (du2[i] > 1.e-3_f * du1[i]) {
            // clang-format off
            return makeFailed("Balsara didn't reduce AV heating\n",
                du1[i], " / ", du2[i],
                "\n divv = ", divv[i], "\n rotv = ", rotv[i]);
            // clang-format on
        }
        if (getLength(dv2[i]) > 1.e-2_f * getLength(dv1[i])) {
            // clang-format off
            return makeFailed("Balsara didn't reduce AV acceleration\n",
                              dv1[i], " / ", dv2[i],
                              "\n divv = ", divv[i], "\n rotv = ", rotv[i]);
            // clang-format on
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, dv1.size());
}

TEST_CASE("Balsara divergent flow", "[av]") {

    // no switch
    EquationHolder term1 = makeTerm<StandardAV>();
    Storage storage1 = Tests::getGassStorage(10000);
    Tests::computeField(storage1, std::move(term1), [](const Vector& r) { return -r; });

    // with switch
    Storage storage2 = Tests::getGassStorage(10000);
    EquationHolder term2 = makeTerm<BalsaraSwitch<StandardAV>>(RunSettings::getDefaults());
    // need to compute twice, first to get velocity divergence and rotation, second to compute AV
    Tests::computeField(storage2, std::move(term2), [](const Vector& r) { return -r; }, 2);

    ArrayView<Vector> dv1 = storage1.getD2t<Vector>(QuantityId::POSITIONS);
    ArrayView<Float> du1 = storage1.getDt<Float>(QuantityId::ENERGY);
    ArrayView<Vector> dv2 = storage2.getD2t<Vector>(QuantityId::POSITIONS);
    ArrayView<Float> du2 = storage2.getDt<Float>(QuantityId::ENERGY);
    ArrayView<Float> divv = storage2.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
    ArrayView<Vector> rotv = storage2.getValue<Vector>(QuantityId::VELOCITY_ROTATION);
    ArrayView<Vector> r = storage2.getValue<Vector>(QuantityId::POSITIONS);

    auto test = [&](const Size i) {
        if (getLength(r[i]) >= 0.7_f) {
            return SUCCESS;
        }
        if (du2[i] != approx(du1[i], 1.e-4_f)) {
            // clang-format off
            return makeFailed("Balsara changed AV heating in divergent flow\n",
                              du1[i], " != ", du2[i],
                              "\n divv = ", divv[i], "\n rotv = ", rotv[i]);
            // clang-format on
        }
        if (dv2[i] != approx(dv1[i], 1.e-4_f)) {
            // clang-format off
            return makeFailed("Balsara changed AV acceleration in divergent flow\n",
                              dv1[i], " != ", dv2[i],
                              "\n divv = ", divv[i], "\n rotv = ", rotv[i]);
            // clang-format on
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, dv1.size());
}
