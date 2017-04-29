#include "sph/av/Balsara.h"
#include "catch.hpp"
#include "sph/av/Standard.h"
#include "utils/SequenceTest.h"
#include "utils/Setup.h"

using namespace Sph;

/*template <typename TFunctor>
void setupBalsara(Storage& storage, BalsaraSwitch<StandardAV>& balsaraAv, TFunctor&& functor) {
    HexagonalPacking distribution;
    const Size N = 10000;
    SphericalDomain domain(Vector(0._f), 1._f);
    storage.insert<Vector, OrderEnum::SECOND>(QuantityId::POSITIONS, distribution.generate(N, domain));
    balsaraAv.initialize(storage, BodySettings::getDefaults());
    storage.insert<Float, OrderEnum::ZERO>(QuantityId::DENSITY, 100._f);
    storage.insert<Float, OrderEnum::ZERO>(
        QuantityId::MASSES, 100._f * domain.getVolume() / storage.getParticleCnt());
    storage.insert<Float, OrderEnum::ZERO>(QuantityId::SOUND_SPEED, 1.e-4_f);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
    for (Size i = 0; i < r.size(); ++i) {
        v[i] = functor(r[i]);
    }

    // compute velocity divergence and rotation
    balsaraAv.update(storage);
    std::unique_ptr<Abstract::Finder> finder = Factory::getFinder(RunSettings::getDefaults());
    finder->build(r);
    LutKernel<3> kernel = Factory::getKernel<3>(RunSettings::getDefaults());
    Array<NeighbourRecord> neighs;
    for (Size i = 0; i < r.size(); ++i) {
        finder->findNeighbours(i, r[i][H] * kernel.radius(), neighs, FinderFlags::FIND_ONLY_SMALLER_H);
        for (NeighbourRecord& n : neighs) {
            const Size j = n.index;
            const Vector grad = kernel.grad(r[i] - r[j], r[i][H]);
            balsaraAv.accumulate(i, j, grad);
        }
    }
    balsaraAv.integrate(storage);
    balsaraAv.update(storage);
}
*/
TEST_CASE("Balsara shear flow", "[av]") {

    // no switch
    EquationHolder term1 = makeTerm<StandardAV>(RunSettings::getDefaults());
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
            return makeFailed("Balsara didn't reduce AV heating\n",
                du1[i],
                " / ",
                du2[i],
                "\n divv = ",
                divv[i],
                "\n rotv = ",
                rotv[i]);
        }
        if (getLength(dv1[i]) > 1.e-5_f && getLength(dv2[i]) > 1.e-3_f * getLength(dv1[i])) {
            return makeFailed("Balsara didn't reduce AV acceleration");
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, dv1.size());
}
/*    setupBalsara(storage, balsaraAv, [](const Vector& r) {
        // spin-up particles with some differential rotation
        const Vector l(r[X], r[Y], 0._f);
        return cross(Vector(0, 0, 1), l) / (getSqrLength(l) + 1._f);
    });

    // check that artificial viscosity is non-zero and that it drops to zero after applying Balsara switch
    StandardAV standardAv(RunSettings::getDefaults());
    standardAv.update(storage);

    double origSum = 0., reducedSum = 0.;
    std::unique_ptr<Abstract::Finder> finder = Factory::getFinder(RunSettings::getDefaults());
    ArrayView<Vector> r, rotv;
    ArrayView<Float> divv;
    r = storage.getValue<Vector>(QuantityId::POSITIONS);
    divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
    rotv = storage.getValue<Vector>(QuantityId::VELOCITY_ROTATION);
    finder->build(r);
    LutKernel<3> kernel = Factory::getKernel<3>(RunSettings::getDefaults());
    Array<NeighbourRecord> neighs;

    auto test = [&](const Size i) {
        if (getLength(r[i]) >= 0.7_f) {
            return SUCCESS;
        }
        finder->findNeighbours(i, r[i][H] * kernel.radius(), neighs, FinderFlags::FIND_ONLY_SMALLER_H);
        for (NeighbourRecord& n : neighs) {
            const Size j = n.index;
            const Float orig = standardAv(i, j);
            const Float reduced = balsaraAv(i, j);
            if (reduced > orig) {
                return makeFailed("Balsara increased AV!");
            }
            if (orig > 1.e-5_f && reduced > 1.e-3_f * orig) {
                // clang-format off
                return makeFailed("Balsara didn't reduce AV!",
                                  "\n AV_orig = ", orig, ", AV_reduced = ", reduced,
                                  "\n (r_i, r_j): ", r[i], ", ", r[j],
                                  "\n (divv_i, divv_j): ", divv[i], ", ", divv[j],
                                  "\n (rotv_i, divv_j): ", rotv[i], ", ", rotv[j],
                                  "\n (f_i, f_j): ", balsaraAv.getFactor(i), ", ",
balsaraAv.getFactor(j));
                // clang-format on
            }
            origSum += orig;
            reducedSum += reduced;
        }
        return SUCCESS;
    };

    REQUIRE_SEQUENCE(test, 0, r.size());

    REQUIRE(origSum > 1e-2);
    REQUIRE(reducedSum < 1.e-5);
}


TEST_CASE("Balsara divergent flow", "[av]") {
    BalsaraSwitch<StandardAV> balsaraAv(RunSettings::getDefaults());
    Storage storage;
    setupBalsara(storage, balsaraAv, [](const Vector& r) {
        // inward motion with no rotation
        return -r;
    });

    // check that Balsara does not affect artificial viscosity
    StandardAV standardAv(RunSettings::getDefaults());
    standardAv.update(storage);
    double origSum = 0., reducedSum = 0.;
    std::unique_ptr<Abstract::Finder> finder = Factory::getFinder(RunSettings::getDefaults());
    ArrayView<Vector> r;
    ArrayView<Float> divv;
    ArrayView<Vector> rotv;
    r = storage.getValue<Vector>(QuantityId::POSITIONS);
    divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
    rotv = storage.getValue<Vector>(QuantityId::VELOCITY_ROTATION);

    finder->build(r);
    LutKernel<3> kernel = Factory::getKernel<3>(RunSettings::getDefaults());
    Array<NeighbourRecord> neighs;
    auto test2 = [&](const Size i) {
        if (getLength(r[i]) >= 0.7_f) {
            return SUCCESS;
        }
        finder->findNeighbours(i, r[i][H] * kernel.radius(), neighs, FinderFlags::FIND_ONLY_SMALLER_H);
        for (NeighbourRecord& n : neighs) {
            const Size j = n.index;
            const Float orig = standardAv(i, j);
            const Float reduced = balsaraAv(i, j);
            if (reduced != approx(orig, 1.e-6_f)) {
                // clang-format off
                return makeFailed("Balsara changed AV in divergent flow! \n",
                                  "AV_orig = ", orig, ", AV_reduced = ", reduced,
                                  "\n (divv_i, divv_j): ", divv[i], ", ", divv[j],
                                  "\n (rotv_i, divv_j): ", rotv[i], ", ", rotv[j],
                                  "\n (f_i, f_j): ", balsaraAv.getFactor(i), ", ",
balsaraAv.getFactor(j));
                // clang-format on
            }
            origSum += orig;
            reducedSum += reduced;
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test2, 0, r.size());
    REQUIRE(origSum > 1e-2);
    REQUIRE(almostEqual(origSum, reducedSum, 1.e-5_f));
}*/
