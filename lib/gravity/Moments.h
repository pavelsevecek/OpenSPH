#pragma once

#include "geometry/Multipole.h"

NAMESPACE_SPH_BEGIN

INLINE Float greenGamma(const Size M, const Float invDistSqr) {
    if (M == 0) {
        return -sqrt(invDistSqr);
    } else {
        return -(2 * M - 1) * invDistSqr * greenGamma(M - 1, invDistSqr);
    }
}


template <Size M>
struct ComputeTrace;

template <>
struct ComputeTrace<1> {
    template <Size N>
    INLINE static Multipole<N - 2> make(const Multipole<N>& m) {
        return MakeMultipole<N - 2>::make(MomentOperators::makeContraction(m));
    }
};

template <>
struct ComputeTrace<2> {
    template <Size N>
    INLINE static Multipole<N - 4> make(const Multipole<N>& m) {
        return MakeMultipole<N - 4>::make(
            MomentOperators::makeContraction(MomentOperators::makeContraction(m)));
    }
};

/// Computes M-fold trace of given multipole moment
template <Size M, Size N>
Multipole<N - 2 * M> computeTrace(const Multipole<N>& m) {
    return ComputeTrace<M>::make(m);
}

template <Size N, Size M>
INLINE SignedSize reducedFactor() {
    return (isOdd(M) ? -1 : 1) *
           SignedSize(doubleFactorial(2 * N - 2 * M - 1) / (factorial(M) * doubleFactorial(2 * N - 1)));
}

template <Size N>
TracelessMultipole<N> computeReducedMultipole(const Multipole<N>& m);

template <>
TracelessMultipole<4> computeReducedMultipole(const Multipole<4>& m) {
    // compute traces
    const Multipole<4>& T0 = m;
    Multipole<2> T1 = computeTrace<1>(m);
    Float T2 = computeTrace<2>(m).value();

    // compute factors
    const Float f0 = reducedFactor<4, 0>();
    const Float f1 = reducedFactor<4, 1>();
    const Float f2 = reducedFactor<4, 2>();

    using namespace MomentOperators;

    // sum up the terms
    return makeTracelessMultipole<4>(T0 * f0 + makePermutations(Delta<2>{}, T1) * f1 + Delta<4>{} * T2 * f2);
}

template <>
TracelessMultipole<3> computeReducedMultipole(const Multipole<3>& m) {
    const Multipole<3>& T0 = m;
    Multipole<1> T1 = computeTrace<1>(m);

    const Float f0 = reducedFactor<3, 0>();
    const Float f1 = reducedFactor<3, 1>();

    using namespace MomentOperators;
    return makeTracelessMultipole<3>(T0 * f0 + makePermutations(Delta<2>{}, T1) * f1);
}

template <>
TracelessMultipole<2> computeReducedMultipole(const Multipole<2>& m) {
    const Multipole<2>& T0 = m;
    Multipole<0> T1 = computeTrace<1>(m);

    const Float f0 = reducedFactor<2, 0>();
    const Float f1 = reducedFactor<2, 1>();

    using namespace MomentOperators;
    return makeTracelessMultipole<2>(T0 * f0 + makePermutations(Delta<2>{}, T1) * f1);
}

template <>
TracelessMultipole<1> computeReducedMultipole(const Multipole<1>& m) {
    const Multipole<1>& T0 = m;

    const Float f0 = reducedFactor<1, 0>();

    using namespace MomentOperators;
    return makeTracelessMultipole<1>(T0 * f0);
}

template <>
TracelessMultipole<0> computeReducedMultipole(const Multipole<0>& m) {
    const Multipole<0>& T0 = m;

    const Float f0 = reducedFactor<0, 0>();

    using namespace MomentOperators;
    return makeTracelessMultipole<0>(T0 * f0);
}

template <Size M, Size N>
TracelessMultipole<M> computeSimplifiedMultipole(const Multipole<N>& q, const Vector& r) {
    static_assert(M <= N, "Incorrect parameters");
    using namespace MomentOperators;
    OuterProduct<1> dr{ r };
    return 1._f / factorial(N - M) * makeInner<N - M>(dr, q);
}


namespace Detail {
    template <Size N>
    INLINE Multipole<N> computeMultipoleImpl(const Vector& dr, const Float m) {
        return makeMultipole<N>(MomentOperators::OuterProduct<N>{ dr }) * m;
    }

    template <>
    INLINE Multipole<0> computeMultipoleImpl(const Vector& UNUSED(dr), const Float m) {
        return m;
    }
}

template <Size N>
Multipole<N> computeMultipole(ArrayView<const Vector> r, const Vector& r0, ArrayView<const Float> m) {
    Multipole<N> moments(0._f);
    for (Size i = 0; i < r.size(); ++i) {
        const Vector dr = r[i] - r0;
        moments += Detail::computeMultipoleImpl<N>(dr, m[i]);
    }
    return moments;
}

template <Size N>
struct GravityEvaluator {
    Vector dr;
    const MultipoleExpansion<N>& ms;
    const StaticArray<Float, N + 1>& gamma;
    Vector& a;

    template <Size M>
    INLINE void visit() {
        const TracelessMultipole<M>& q = ms.template getOrder<M>();
        const Float Q0 = computeSimplifiedMultipole<0>(q).value();
        const Vector Q1 = computeSimplifiedMultipole<1>(q).vector();
        a += gamma[M + 1] * dr * Q0 + gamma[M] * Q1;
    }
};


template <Size N>
Vector evaluateGravity(const Vector& dr, const MultipoleExpansion<N>& ms) {
    StaticArray<Float, N + 1> gamma;
    const Float invDistSqr = 1._f / getSqrLength(dr);
    for (Size i = 0; i < N + 1; ++i) {
        gamma[i] = greenGamma(i, invDistSqr);
    }

    Vector a(0._f);
    GravityEvaluator<N> evaluator{ dr, ms, gamma, a };
    staticFor<0, N - 1>(evaluator);
    ASSERT(isReal(a));
    return a;
}

/*
TracelessMultipole<1> parallelAxisTheorem(const TracelessMultipole<1>& Q) {
    return Q;
}

TracelessMultipole<2> parallelAxisTheorem(const TracelessMultipole<2>& Qij, const Float Q, const Vector& d) {
    using namespace MomentOperators;
    return makeTracelessMultipole<2>(Qij + OuterProduct<2>{ d } * Q);
}

TracelessMultipole<3> parallelAxisTheorem(const TracelessMultipole<3>& Qijk,
    const TracelessMultipole<2>& Qij,
    const Float Q,
    const Vector& d) {
    using namespace MomentOperators;
    OuterProduct<1> d1{ d };
    // it saves const l-value reference internally, it should survive even for temporary objects
    OuterProduct<1> d2{ 2._f / 5._f * d };
    return makeTracelessMultipole<3>(Qijk + OuterProduct<3>{ d } * Q + makePermutations(Qij, d1) -
                                     makeMultiply(makePermutations(Delta<2>{}, Qij), d2));
}*/


NAMESPACE_SPH_END
