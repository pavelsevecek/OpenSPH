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
    const Size N = 10000;
    SphericalDomain domain(Vector(0._f), 1._f);
    storage.emplace<Vector, OrderEnum::SECOND_ORDER>(
        QuantityKey::POSITIONS, distribution.generate(N, domain));
    balsaraAv.initialize(storage, BODY_SETTINGS);
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::DENSITY, 100._f);
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(
        QuantityKey::MASSES, 100._f * domain.getVolume() / storage.getParticleCnt());
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::SOUND_SPEED, 1.e-4_f);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityKey::POSITIONS);
    for (Size i = 0; i < r.size(); ++i) {
        v[i] = functor(r[i]);
    }

    // compute velocity divergence and rotation
    balsaraAv.update(storage);
    std::unique_ptr<Abstract::Finder> finder = Factory::getFinder(GLOBAL_SETTINGS);
    finder->build(r);
    LutKernel<3> kernel = Factory::getKernel<3>(GLOBAL_SETTINGS);
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
    BalsaraSwitch<StandardAV> balsaraAv(GLOBAL_SETTINGS);
    Storage storage;
    setupBalsara(storage, balsaraAv, [](const Vector& r) {
        // spin-up particles with some differential rotation
        const Vector l (r[X], r[Y], 0._f);
        return cross(Vector(0, 0, 1), l) / (getSqrLength(l) + 1._f);
    });

    // check that artificial viscosity is non-zero and that it drops to zero after applying Balsara switch
    StandardAV standardAv(GLOBAL_SETTINGS);
    standardAv.update(storage);
    bool allMatching = true;
    double origSum = 0., reducedSum = 0.;
    std::unique_ptr<Abstract::Finder> finder = Factory::getFinder(GLOBAL_SETTINGS);
    ArrayView<Vector> r, rotv;
    ArrayView<Float> divv;
    r  = storage.getValue<Vector>(QuantityKey::POSITIONS);
    divv = storage.getValue<Float>(QuantityKey::VELOCITY_DIVERGENCE);
    rotv = storage.getValue<Vector>(QuantityKey::VELOCITY_ROTATION);
    finder->build(r);
    LutKernel<3> kernel = Factory::getKernel<3>(GLOBAL_SETTINGS);
    Array<NeighbourRecord> neighs;
    for (Size i = 0; i < r.size(); ++i) {
        if (getLength(r[i]) >= 0.7_f) {
            continue;
        }
        if (!allMatching) {
            break;
        }
        finder->findNeighbours(i, r[i][H] * kernel.radius(), neighs, FinderFlags::FIND_ONLY_SMALLER_H);
        for (NeighbourRecord& n : neighs) {
            const Size j = n.index;
            const Float orig = standardAv(i, j);
            const Float reduced = balsaraAv(i, j);
            if (reduced > orig) {
                std::cout << "Balsara increased AV!";
                allMatching = false;
                break;
            }
            if (orig > EPS && reduced > 1.e-3_f * orig) {
                std::cout << "Balsara didn't reduce AV!" << std::endl;
                std::cout << "particle: i = " << i << std::endl;
                std::cout << "          r = " << r[i] << " & " << r[j] << std::endl;
                std::cout << "          orig = " << orig << ", reduced = " << reduced << std::endl;
                std::cout << "          divv = " << divv[i] << " & " << divv[j] << std::endl;
                std::cout << "          rotv = " << rotv[i] << " & " << rotv[j] << std::endl;
                std::cout << "Balsara factor: " << balsaraAv.getFactor(i) << " & " << balsaraAv.getFactor(j) << std::endl;
                allMatching = false;
                break;
            }
            origSum += orig;
            reducedSum += reduced;
        }
    }
    REQUIRE(allMatching);
    REQUIRE(origSum > 1e-2);
    REQUIRE(reducedSum < 1.e-5);
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
    ArrayView<Vector> rotv;
    r = storage.getValue<Vector>(QuantityKey::POSITIONS);
    divv = storage.getValue<Float>(QuantityKey::VELOCITY_DIVERGENCE);
    rotv = storage.getValue<Vector>(QuantityKey::VELOCITY_ROTATION);
    for (Size i = 0; i < divv.size(); ++i) {
        // particles on boundary have different velocity divergence, check only particles inside
        if (getLength(r[i]) < 0.7_f) {
            if (!Math::almostEqual(divv[i], -3._f, 0.1_f)) {
                std::cout << "Incorrect velocity divergence: " << divv[i] << " / -3" << std::endl;
                std::cout << "particle: r = " << r[i] << std::endl;
                allMatching = false;
                break;
            }
            if (!Math::almostEqual(rotv[i], Vector(0._f), 0.1_f)) {
                std::cout << "Incorrect velocity rotation: " << rotv[i] << " / Vector(0.f) " << std::endl;
                std::cout << "particle: r = " << r[i] << std::endl;
                allMatching = false;
                break;
            }
        }
    }

    finder->build(r);
    LutKernel<3> kernel = Factory::getKernel<3>(GLOBAL_SETTINGS);
    Array<NeighbourRecord> neighs;
    for (Size i = 0; i < r.size(); ++i) {
        if (getLength(r[i]) >= 0.7_f) {
            continue;
        }
        finder->findNeighbours(i, r[i][H] * kernel.radius(), neighs, FinderFlags::FIND_ONLY_SMALLER_H);
        for (NeighbourRecord& n : neighs) {
            if (!allMatching) {
                break;
            }
            const Size j = n.index;
            const Float orig = standardAv(i, j);
            const Float reduced = balsaraAv(i, j);
            if (!Math::almostEqual(reduced, orig)) {
                std::cout << "Balsara changed AV in divergent flow! " << reduced << " / " << orig
                          << std::endl;
                std::cout << "particle: i = " << i << std::endl;
                std::cout << "          divv = " << divv[i] << " & " << divv[j] << std::endl;
                std::cout << "          rotv = " << rotv[i] << " & " << rotv[j] << std::endl;
                std::cout << "Balsara factor: " << balsaraAv.getFactor(i) << " & " << balsaraAv.getFactor(j) << std::endl;
                allMatching = false;
                break;
            }
            origSum += orig;
            reducedSum += reduced;
        }
    }
    REQUIRE(allMatching);
    REQUIRE(origSum > 1e-2);
    REQUIRE(Math::almostEqual(origSum, reducedSum, 1.e-5_f));
}
