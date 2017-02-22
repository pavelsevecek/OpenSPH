#include "sph/av/Balsara.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "objects/containers/ArrayUtils.h"
#include "objects/finders/AbstractFinder.h"
#include "sph/av/Standard.h"
#include "sph/initial/Distribution.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;

template <typename TFunctor>
void setupBalsara(Storage& storage, BalsaraSwitch<StandardAV>& balsaraAv, TFunctor&& functor) {
    HexagonalPacking distribution;
    const Size N = 10000;
    SphericalDomain domain(Vector(0._f), 1._f);
    storage.insert<Vector, OrderEnum::SECOND_ORDER>(
        QuantityIds::POSITIONS, distribution.generate(N, domain));
    balsaraAv.initialize(storage, BodySettings::getDefaults());
    storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::DENSITY, 100._f);
    storage.insert<Float, OrderEnum::ZERO_ORDER>(
        QuantityIds::MASSES, 100._f * domain.getVolume() / storage.getParticleCnt());
    storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::SOUND_SPEED, 1.e-4_f);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    for (Size i = 0; i < r.size(); ++i) {
        v[i] = functor(r[i]);
    }

    // compute velocity divergence and rotation
    balsaraAv.update(storage);
    std::unique_ptr<Abstract::Finder> finder = Factory::getFinder(GlobalSettings::getDefaults());
    finder->build(r);
    LutKernel<3> kernel = Factory::getKernel<3>(GlobalSettings::getDefaults());
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

TEST_CASE("Balsara shear flow", "[av]") {
    BalsaraSwitch<StandardAV> balsaraAv(GlobalSettings::getDefaults());
    Storage storage;
    setupBalsara(storage, balsaraAv, [](const Vector& r) {
        // spin-up particles with some differential rotation
        const Vector l(r[X], r[Y], 0._f);
        return cross(Vector(0, 0, 1), l) / (getSqrLength(l) + 1._f);
    });

    // check that artificial viscosity is non-zero and that it drops to zero after applying Balsara switch
    StandardAV standardAv(GlobalSettings::getDefaults());
    standardAv.update(storage);

    double origSum = 0., reducedSum = 0.;
    std::unique_ptr<Abstract::Finder> finder = Factory::getFinder(GlobalSettings::getDefaults());
    ArrayView<Vector> r, rotv;
    ArrayView<Float> divv;
    r = storage.getValue<Vector>(QuantityIds::POSITIONS);
    divv = storage.getValue<Float>(QuantityIds::VELOCITY_DIVERGENCE);
    rotv = storage.getValue<Vector>(QuantityIds::VELOCITY_ROTATION);
    finder->build(r);
    LutKernel<3> kernel = Factory::getKernel<3>(GlobalSettings::getDefaults());
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
                                  "\n (f_i, f_j): ", balsaraAv.getFactor(i), ", ", balsaraAv.getFactor(j));
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
    BalsaraSwitch<StandardAV> balsaraAv(GlobalSettings::getDefaults());
    Storage storage;
    setupBalsara(storage, balsaraAv, [](const Vector& r) {
        // inward motion with no rotation
        return -r;
    });

    // check that Balsara does not affect artificial viscosity
    StandardAV standardAv(GlobalSettings::getDefaults());
    standardAv.update(storage);
    double origSum = 0., reducedSum = 0.;
    std::unique_ptr<Abstract::Finder> finder = Factory::getFinder(GlobalSettings::getDefaults());
    ArrayView<Vector> r;
    ArrayView<Float> divv;
    ArrayView<Vector> rotv;
    r = storage.getValue<Vector>(QuantityIds::POSITIONS);
    divv = storage.getValue<Float>(QuantityIds::VELOCITY_DIVERGENCE);
    rotv = storage.getValue<Vector>(QuantityIds::VELOCITY_ROTATION);

    finder->build(r);
    LutKernel<3> kernel = Factory::getKernel<3>(GlobalSettings::getDefaults());
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
                                  "\n (f_i, f_j): ", balsaraAv.getFactor(i), ", ", balsaraAv.getFactor(j));
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
}
