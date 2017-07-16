#pragma once

#include "geometry/Multipole.h"
#include "objects/finders/KdTree.h"

NAMESPACE_SPH_BEGIN

inline Float greenGamma(const Size M, const Float invDistSqr) {
    if (M == 0) {
        const Float g0 = -sqrt(invDistSqr);
        return g0;
    } else {
        const Float g_prev = greenGamma(M - 1, invDistSqr);
        const Float g = -(2._f * M - 1._f) * invDistSqr * g_prev;
        return g;
    }
}


template <Size M>
struct ComputeTrace;

template <>
struct ComputeTrace<1> {
    template <Size N>
    INLINE static Multipole<N - 2> make(const Multipole<N>& m) {
        static_assert(N >= 2, "invalid parameter");
        return makeMultipole<N - 2>(MomentOperators::makeContraction(m));
    }
};

template <>
struct ComputeTrace<2> {
    template <Size N>
    INLINE static Multipole<N - 4> make(const Multipole<N>& m) {
        static_assert(N >= 4, "invalid parameter");
        return makeMultipole<N - 4>(MomentOperators::makeContraction(MomentOperators::makeContraction(m)));
    }
};

/// Computes M-fold trace of given multipole moment
template <Size M, Size N>
Multipole<N - 2 * M> computeTrace(const Multipole<N>& m) {
    static_assert(N >= 2 * M, "invalid parameter");
    return ComputeTrace<M>::make(m);
}

template <Size N, Size M>
INLINE Float reducedFactor() {
    static_assert(N > 0, "Cannot be used for N == 0");
    static_assert(2 * N - 2 * M > 0, "invalid parameter");
    const SignedSize sign = (isOdd(M) ? -1 : 1);
    const Float num = doubleFactorial(2 * N - 2 * M - 1);
    const Float denom = factorial(M) * doubleFactorial(2 * N - 1);
    const Float factor = sign * num / denom;
    return factor;
}

template <Size N>
TracelessMultipole<N> computeReducedMultipole(const Multipole<N>& m);

template <>
inline TracelessMultipole<4> computeReducedMultipole(const Multipole<4>& m) {
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
inline TracelessMultipole<3> computeReducedMultipole(const Multipole<3>& m) {
    const Multipole<3>& T0 = m;
    Multipole<1> T1 = computeTrace<1>(m);

    const Float f0 = reducedFactor<3, 0>();
    const Float f1 = reducedFactor<3, 1>();

    using namespace MomentOperators;
    return makeTracelessMultipole<3>(T0 * f0 + makePermutations(Delta<2>{}, T1) * f1);
}

template <>
inline TracelessMultipole<2> computeReducedMultipole(const Multipole<2>& m) {
    const Multipole<2>& T0 = m;
    Multipole<0> T1 = computeTrace<1>(m);

    const Float f0 = reducedFactor<2, 0>();
    const Float f1 = reducedFactor<2, 1>();

    using namespace MomentOperators;
    return makeTracelessMultipole<2>(T0 * f0 + makePermutations(Delta<2>{}, T1) * f1);
}

template <>
inline TracelessMultipole<1> computeReducedMultipole(const Multipole<1>& m) {
    const Multipole<1>& T0 = m;

    const Float f0 = reducedFactor<1, 0>();

    using namespace MomentOperators;
    return makeTracelessMultipole<1>(T0 * f0);
}

template <>
inline TracelessMultipole<0> computeReducedMultipole(const Multipole<0>& m) {
    return makeTracelessMultipole<0>(m);
}

template <Size M, Size N>
std::enable_if_t<(M < N), TracelessMultipole<M>> computeMultipolePotential(const TracelessMultipole<N>& q,
    const Vector& r) {
    static_assert(M <= N, "Incorrect parameters");
    using namespace MomentOperators;
    OuterProduct<N - M> dr{ r };
    return makeTracelessMultipole<M>(makeInner<N - M>(dr, q) * (1._f / factorial(N - M)));
}

template <Size M, Size N>
std::enable_if_t<M == N, TracelessMultipole<M>> computeMultipolePotential(const TracelessMultipole<N>& q,
    const Vector& UNUSED(r)) {
    return q;
}

template <Size M, Size N>
std::enable_if_t<(M > N), TracelessMultipole<M>> computeMultipolePotential(
    const TracelessMultipole<N>& UNUSED(q),
    const Vector& UNUSED(r)) {
    return TracelessMultipole<M>(0._f);
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

template <Size N, typename TSequence>
Multipole<N> computeMultipole(ArrayView<const Vector> r,
    ArrayView<const Float> m,
    const Vector& r0,
    const TSequence& sequence) {
    Multipole<N> moments(0._f);
    for (Size i : sequence) {
        const Vector dr = r[i] - r0;
        moments += Detail::computeMultipoleImpl<N>(dr, m[i]);
    }
    return moments;
}

INLINE TracelessMultipole<1> parallelAxisTheorem(const TracelessMultipole<1>& Qi,
    const Float Q,
    const Vector& d) {
    using namespace MomentOperators;
    return makeTracelessMultipole<1>(Qi + OuterProduct<1>{ d } * Q);
}

INLINE TracelessMultipole<2> parallelAxisTheorem(const TracelessMultipole<2>& Qij,
    const Float Q,
    const Vector& d) {
    using namespace MomentOperators;
    const Multipole<2> d2 = makeMultipole<2>(OuterProduct<2>{ d });
    const TracelessMultipole<2> f2 = computeReducedMultipole(d2);
    return makeTracelessMultipole<2>(Qij + f2 * Q);
}

namespace MomentOperators {
    struct Term2 {
        const TracelessMultipole<2>& Q;
        const Vector& d;

        template <Size I, Size J, Size K, Size L>
        INLINE Float perm() const {
            const Delta<2> delta;
            return delta.value<I, J>() * Q.value<K, L>() + delta.value<I, K>() * Q.value<J, L>() +
                   delta.value<J, K>() * Q.value<I, L>();
        }

        template <Size I, Size J, Size K>
        INLINE Float value() const {
            return -2._f / 5._f *
                   (perm<I, J, K, 0>() * d[0] + perm<I, J, K, 1>() * d[1] + perm<I, J, K, 2>() * d[2]);
        }
    };

    struct Term30 {
        const TracelessMultipole<3>& Q;
        const Vector& d;

        template <Size I, Size J, Size K, Size L, Size M>
        INLINE Float perm() const {
            const Delta<2> delta;
            return delta.value<I, J>() * Q.value<K, L, M>() + delta.value<I, K>() * Q.value<J, L, M>() +
                   delta.value<I, L>() * Q.value<J, K, M>() + delta.value<J, K>() * Q.value<I, L, M>() +
                   delta.value<J, L>() * Q.value<I, K, M>() + delta.value<K, L>() * Q.value<I, J, M>();
        }

        template <Size I, Size J, Size K, Size L>
        INLINE Float value() const {
            return perm<I, J, K, L, 0>() * d[0] + perm<I, J, K, L, 1>() * d[1] + perm<I, J, K, L, 2>() * d[2];
        }
    };

    struct Term31 {
        const TracelessMultipole<2>& Q;
        const TracelessMultipole<2>& f2;

        const static Delta<2> delta1, delta2;

        template <Size I, Size J, Size K, Size L, Size M, Size N>
        INLINE Float ddq() const {
            return delta1.template value<I, J>() * delta2.template value<K, M>() * Q.template value<L, N>();
        }

        template <Size I, Size J, Size K, Size L, Size M, Size N>
        INLINE Float perm() const {
#if 0
            return ddq<I, J, K, L, M, N>() + ddq<I, J, L, K, M, N>() + ddq<I, L, J, K, M, N>() +
                   ddq<I, L, K, J, M, N>() + ddq<I, K, J, L, M, N>() + ddq<I, K, L, J, M, N>() +
                   ddq<J, K, L, I, M, N>() + ddq<J, K, I, L, M, N>() + ddq<J, L, I, K, M, N>() +
                   ddq<J, L, K, I, M, N>() + ddq<K, L, I, J, M, N>() + ddq<K, L, J, I, M, N>();
#else
            return ddq<I, J, K, L, M, N>() + ddq<I, L, J, K, M, N>() + ddq<I, K, J, L, M, N>() +
                   ddq<J, K, L, I, M, N>() + ddq<J, L, I, K, M, N>() + ddq<K, L, I, J, M, N>();
#endif
        }

        template <Size I, Size J, Size K, Size L>
        INLINE Float value() const {
            return perm<I, J, K, L, 0, 0>() * f2.template value<0, 0>() +
                   perm<I, J, K, L, 0, 1>() * f2.template value<0, 1>() +
                   perm<I, J, K, L, 0, 2>() * f2.template value<0, 2>() +
                   perm<I, J, K, L, 1, 0>() * f2.template value<1, 0>() +
                   perm<I, J, K, L, 1, 1>() * f2.template value<1, 1>() +
                   perm<I, J, K, L, 1, 2>() * f2.template value<1, 2>() +
                   perm<I, J, K, L, 2, 0>() * f2.template value<2, 0>() +
                   perm<I, J, K, L, 2, 1>() * f2.template value<2, 1>() +
                   perm<I, J, K, L, 2, 2>() * f2.template value<2, 2>();
        }
    };

    struct Term32 {
        const TracelessMultipole<2>& Q;
        const TracelessMultipole<2>& f2;

        template <Size I, Size J, Size K, Size L>
        INLINE Float value() const {
            return makePermutations(Delta<2>{}, Delta<2>{}).template value<I, J, K, L>() *
                   makeInner<2>(Q, f2).value() * (-1._f / 5._f);
        }
    };
}

INLINE TracelessMultipole<3> parallelAxisTheorem(const TracelessMultipole<3>& Qijk,
    const TracelessMultipole<2>& Qij,
    const Float Q,
    const Vector& d) {
    using namespace MomentOperators;
    const OuterProduct<1> d1{ d };
    const Multipole<3> d3 = makeMultipole<3>(OuterProduct<3>{ d });
    const TracelessMultipole<3> f3 = computeReducedMultipole(d3);
    return makeTracelessMultipole<3>(Qijk + f3 * Q + makePermutations(Qij, d1) + Term2{ Qij, d });
}


INLINE TracelessMultipole<4> parallelAxisTheorem(const TracelessMultipole<4>& Qijkl,
    const TracelessMultipole<3>& Qijk,
    const TracelessMultipole<2>& Qij,
    const Float Q,
    const Vector& d) {
    using namespace MomentOperators;
    OuterProduct<1> d1{ d };
    OuterProduct<2> d2{ d };
    OuterProduct<4> d4{ d };
    const TracelessMultipole<2> f2 = computeReducedMultipole(makeMultipole<2>(d2));
    const TracelessMultipole<4> f4 = computeReducedMultipole(makeMultipole<4>(d4));

    return makeTracelessMultipole<4>(
        Qijkl + f4 * Q + makePermutations(Qijk, d1) + makePermutations(Qij, f2) +
        (Term30{ Qijk, d } + Term31{ Qij, f2 } + Term32{ Qij, f2 }) * (-2._f / 7._f));
}

template <Size M, Size N>
INLINE Vector computeMultipoleAcceleration(const MultipoleExpansion<N>& ms,
    ArrayView<const Float> gamma,
    const Vector& dr) {
    const TracelessMultipole<M>& q = ms.template order<M>();
    const Float Q0 = computeMultipolePotential<0>(q, dr).value();
    const Vector Q1 = computeMultipolePotential<1>(q, dr).vector();
    const Vector a = gamma[M + 1] * dr * Q0 + gamma[M] * Q1;
    ASSERT(isReal(a), dr, Q0, Q1, gamma);
    return a;
}

template <Size N>
Vector evaluateGravity(const Vector& dr, const MultipoleExpansion<N>& ms, const Size maxOrder) {
    StaticArray<Float, N + 2> gamma;
    const Float invDistSqr = 1._f / getSqrLength(dr);
    for (Size i = 0; i < N + 2; ++i) {
        gamma[i] = greenGamma(i, invDistSqr);
    }

    Vector a(0._f);
    switch (maxOrder) {
    case 3: // octupole
        a += computeMultipoleAcceleration<3>(ms, gamma, -dr);
        SPH_FALLTHROUGH;
    case 2: // quadrupole
        a += computeMultipoleAcceleration<2>(ms, gamma, -dr);
        SPH_FALLTHROUGH;
    case 0: // monopole
        a += computeMultipoleAcceleration<0>(ms, gamma, -dr);
        break;
    default:
        NOT_IMPLEMENTED;
    };

    ASSERT(isReal(a) && getSqrLength(a) > 0._f);
    return a;
}


NAMESPACE_SPH_END
