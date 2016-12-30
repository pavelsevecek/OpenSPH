#include "sph/av/Balsara.h"
#include "catch.hpp"
#include "objects/containers/ArrayUtils.h"
#include "objects/finders/AbstractFinder.h"
#include "sph/av/Standard.h"
#include "sph/initial/Distribution.h"
#include <iostream>

using namespace Sph;

template <typename TFunctor>
void setupBalsara(Storage& storage, BalsaraSwitch<StandardAV>& balsaraAv, TFunctor&& functor) {
    HexagonalPacking distribution;
    const int N = 10000;
    SphericalDomain domain(Vector(0._f), 1._f);
    storage.emplace<Vector, OrderEnum::SECOND_ORDER>(
        QuantityKey::POSITIONS, distribution.generate(N, domain));
    balsaraAv.initialize(storage, BODY_SETTINGS);
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::DENSITY, 100._f);
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::MASSES, 100._f * domain.getVolume() / N);
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::SOUND_SPEED, 400._f);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityKey::POSITIONS);
    for (int i = 0; i < r.size(); ++i) {
        v[i] = functor(r[i]);
    }

    // compute velocity divergence and rotation
    balsaraAv.update(storage);
    std::unique_ptr<Abstract::Finder> finder = Factory::getFinder(GLOBAL_SETTINGS);
    finder->build(r);
    LutKernel<3> kernel = Factory::getKernel<3>(GLOBAL_SETTINGS);
    Array<NeighbourRecord> neighs;
    for (int i = 0; i < r.size(); ++i) {
        finder->findNeighbours(i, r[i][H] * kernel.radius(), neighs);
        for (NeighbourRecord& n : neighs) {
            const int j = n.index;
            const Vector grad = kernel.grad(r[i] - r[j], r[i][H]);
            balsaraAv.accumulate(i, j, grad);
        }
    }
    balsaraAv.integrate(storage);
}

TEST_CASE("Balsara shear flow", "[av]") {
    BalsaraSwitch<StandardAV> balsaraAv(GLOBAL_SETTINGS);
    Storage storage;
    setupBalsara(storage, balsaraAv, [](const Vector& r) {
        // spin-up particles with some differential rotation
        const Vector axis(0._f, 0._f, 1._f);
        const Vector l = r - axis * dot(r, axis);
        return cross(axis, l) / (getSqrLength(l) + 1._f);
    });

    // check that artificial viscosity is non-zero and that it drops to zero after applying Balsara switch
    StandardAV standardAv(GLOBAL_SETTINGS);
    standardAv.update(storage);
    bool allMatching = true;
    double origSum = 0., reducedSum = 0.;
    std::unique_ptr<Abstract::Finder> finder = Factory::getFinder(GLOBAL_SETTINGS);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityKey::POSITIONS);
    finder->build(r);
    LutKernel<3> kernel = Factory::getKernel<3>(GLOBAL_SETTINGS);
    Array<NeighbourRecord> neighs;
    for (int i = 0; i < r.size(); ++i) {
        finder->findNeighbours(i, r[i][H] * kernel.radius(), neighs);
        for (NeighbourRecord& n : neighs) {
            const int j = n.index;
            const Float orig = standardAv(i, j);
            const Float reduced = balsaraAv(i, j);
            if (reduced > orig) {
                std::cout << "Balsara increased AV!";
                allMatching = false;
                break;
            }
            if (orig > EPS && reduced > EPS * orig) {
                std::cout << "Balsara didn't reduce AV!";
                allMatching = false;
                break;
            }
            origSum += orig;
            reducedSum += reduced;
        }
    }
    REQUIRE(allMatching);
    REQUIRE(origSum > 1e2);
    REQUIRE(reducedSum < EPS);
}


TEST_CASE("Balsara divergent flow", "[av]") {
    BalsaraSwitch<StandardAV> balsaraAv(GLOBAL_SETTINGS);
    Storage storage;
    setupBalsara(storage, balsaraAv, [](const Vector& r) {
        // inward motion with no rotation
        return -r;
    });

    // check that Balsara does not affect artificial viscosity
    StandardAV standardAv(GLOBAL_SETTINGS);
    standardAv.update(storage);
    bool allMatching = true;
    double origSum = 0., reducedSum = 0.;
    std::unique_ptr<Abstract::Finder> finder = Factory::getFinder(GLOBAL_SETTINGS);
    ArrayView<Vector> r;
    ArrayView<Float> divv;
    r = storage.getValue<Vector>(QuantityKey::POSITIONS);
    divv = storage.getValue<Float>(QuantityKey::VELOCITY_DIVERGENCE);
    for (int i = 0; i < divv.size(); ++i) {
        if (!Math::almostEqual(divv[i], -3._f, 0.5_f)) {
            std::cout << "Incorrect velocity divergence: " << divv[i] << " / -3" << std::endl;
            allMatching = false;
            break;
        }
    }

    finder->build(r);
    LutKernel<3> kernel = Factory::getKernel<3>(GLOBAL_SETTINGS);
    Array<NeighbourRecord> neighs;
    for (int i = 0; i < r.size(); ++i) {
        finder->findNeighbours(i, r[i][H] * kernel.radius(), neighs);
        for (NeighbourRecord& n : neighs) {
            if (!allMatching) {
                break;
            }
            const int j = n.index;
            const Float orig = standardAv(i, j);
            const Float reduced = balsaraAv(i, j);
            if (reduced != orig) {
                std::cout << "Balsara changed AV in divergent flow! " << reduced << " / " << orig;
                allMatching = false;
                break;
            }
            origSum += orig;
            reducedSum += reduced;
        }
    }
    REQUIRE(allMatching);
    REQUIRE(origSum > 1e2);
    REQUIRE(origSum == reducedSum);
}
