#include "sph/equations/av/Balsara.h"
#include "catch.hpp"
#include "sph/equations/av/Standard.h"
#include "sph/solvers/AsymmetricSolver.h"
#include "sph/solvers/SymmetricSolver.h"
#include "tests/Approx.h"
#include "tests/Setup.h"
#include "utils/SequenceTest.h"
#include "utils/Utils.h"

using namespace Sph;

TEMPLATE_TEST_CASE("Balsara shear flow", "[av]", SymmetricSolver, AsymmetricSolver) {
    // no switch
    EquationHolder term1 = makeTerm<StandardAV>();
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 1._f).set(BodySettingsId::ENERGY, 1._f);
    Storage storage1 = Tests::getGassStorage(10000, settings);
    Tests::computeField<TestType>(storage1, std::move(term1), [](const Vector& r) {
        // spin-up particles with some differential rotation
        const Vector l(r[X], r[Y], 0._f);
        return cross(Vector(0, 0, 1), l) / (getSqrLength(l) + 1._f);
    });

    // with switch
    Storage storage2 = Tests::getGassStorage(10000, settings);
    EquationHolder term2 = makeTerm<BalsaraSwitch<StandardAV>>(RunSettings::getDefaults());
    // need to compute twice, first to get velocity divergence and rotation, second to compute AV
    Tests::computeField<TestType>(storage2,
        std::move(term2),
        [](const Vector& r) {
            const Vector l(r[X], r[Y], 0._f);
            return cross(Vector(0, 0, 1), l) / (getSqrLength(l) + 1._f);
        },
        2);

    ArrayView<Vector> dv1 = storage1.getD2t<Vector>(QuantityId::POSITION);
    ArrayView<Float> du1 = storage1.getDt<Float>(QuantityId::ENERGY);
    ArrayView<Vector> dv2 = storage2.getD2t<Vector>(QuantityId::POSITION);
    ArrayView<Float> du2 = storage2.getDt<Float>(QuantityId::ENERGY);
    ArrayView<Float> divv = storage2.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
    ArrayView<Vector> rotv = storage2.getValue<Vector>(QuantityId::VELOCITY_ROTATION);
    ArrayView<Vector> r = storage2.getValue<Vector>(QuantityId::POSITION);

    auto test = [&](const Size i) -> Outcome {
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

TEMPLATE_TEST_CASE("Balsara divergent flow", "[av]", SymmetricSolver, AsymmetricSolver) {
    // no switch
    EquationHolder term1 = makeTerm<StandardAV>();
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 1._f).set(BodySettingsId::ENERGY, 1._f);
    Storage storage1 = Tests::getGassStorage(10000, settings);
    Tests::computeField<TestType>(storage1, std::move(term1), [](const Vector& r) { return -r; });

    // with switch
    Storage storage2 = Tests::getGassStorage(10000, settings);
    EquationHolder term2 = makeTerm<BalsaraSwitch<StandardAV>>(RunSettings::getDefaults());
    // need to compute twice, first to get velocity divergence and rotation, second to compute AV
    Tests::computeField<TestType>(storage2, std::move(term2), [](const Vector& r) { return -r; }, 2);

    ArrayView<Vector> dv1 = storage1.getD2t<Vector>(QuantityId::POSITION);
    ArrayView<Float> du1 = storage1.getDt<Float>(QuantityId::ENERGY);
    ArrayView<Vector> dv2 = storage2.getD2t<Vector>(QuantityId::POSITION);
    ArrayView<Float> du2 = storage2.getDt<Float>(QuantityId::ENERGY);
    ArrayView<Float> divv = storage2.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
    ArrayView<Vector> rotv = storage2.getValue<Vector>(QuantityId::VELOCITY_ROTATION);
    ArrayView<Vector> r = storage2.getValue<Vector>(QuantityId::POSITION);

    auto test = [&](const Size i) -> Outcome {
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
