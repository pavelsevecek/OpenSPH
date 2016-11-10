#pragma once

#include "storage/Quantity.h"

NAMESPACE_SPH_BEGIN


template <TemporalEnum Type>
struct StorageIterator;

template <>
struct StorageIterator<TemporalEnum::ALL> {
    template <typename TFunctor>
    static void iterate(Array<Quantity>& qs, TFunctor&& functor) {
        for (auto& q : qs) {
            for (auto& i : q.template getBuffers<Float>()) {
                functor(i);
            }
            for (auto& i : q.template getBuffers<Vector>()) {
                functor(i);
            }
        }
    }

    template <typename TFunctor>
    static void iteratePair(Array<Quantity>& qs1, Array<Quantity>& qs2, TFunctor&& functor) {
        ASSERT(qs1.size() == qs2.size());
        for (int i = 0; i < qs1.size(); ++i) {
            auto scalars1 = qs1[i].template getBuffers<Float>();
            auto scalars2 = qs2[i].template getBuffers<Float>();
            ASSERT(scalars1.size() == scalars2.size());
            for (int j = 0; j < scalars1.size(); ++j) {
                functor(scalars1[j], scalars2[j]);
            }
            auto vectors1 = qs1[i].template getBuffers<Vector>();
            auto vectors2 = qs2[i].template getBuffers<Vector>();
            ASSERT(vectors1.size() == vectors2.size());
            for (int j = 0; j < vectors1.size(); ++j) {
                functor(vectors1[j], vectors2[j]);
            }
        }
    }
};

template <>
struct StorageIterator<TemporalEnum::FIRST_ORDER> {
    template <typename TFunctor>
    static void iterate(Array<Quantity>& qs, TFunctor&& functor) {
        for (auto& q : qs) {
            if (q.getTemporalEnum() != TemporalEnum::FIRST_ORDER) {
                continue;
            }
            auto scalar = q.cast<Float, TemporalEnum::FIRST_ORDER>();
            if (scalar) {
                functor(scalar->getValue(), scalar->getDerivative());
            } else {
                auto vector = q.cast<Vector, TemporalEnum::FIRST_ORDER>();
                ASSERT(vector);
                functor(vector->getValue(), vector->getDerivative());
            }
        }
    }

    template <typename TFunctor>
    static void iteratePair(Array<Quantity>& qs1, Array<Quantity>& qs2, TFunctor&& functor) {
        ASSERT(qs1.size() == qs2.size());
        for (int i = 0; i < qs1.size(); ++i) {
            if (qs1[i].getTemporalEnum() != TemporalEnum::FIRST_ORDER) {
                continue;
            }
            ASSERT(qs2[i].getTemporalEnum() == TemporalEnum::FIRST_ORDER);
            auto scalar1 = qs1[i].cast<Float, TemporalEnum::FIRST_ORDER>();
            auto scalar2 = qs2[i].cast<Float, TemporalEnum::FIRST_ORDER>();
            if (scalar1 && scalar2) {
                functor(scalar1->getValue(),
                        scalar1->getDerivative(),
                        scalar2->getValue(),
                        scalar2->getDerivative());
            } else {
                auto vector1 = qs1[i].cast<Vector, TemporalEnum::FIRST_ORDER>();
                auto vector2 = qs2[i].cast<Vector, TemporalEnum::FIRST_ORDER>();
                ASSERT(vector1 && vector2);
                functor(vector1->getValue(),
                        vector1->getDerivative(),
                        vector2->getValue(),
                        vector2->getDerivative());
            }
        }
    }
};

template <>
struct StorageIterator<TemporalEnum::SECOND_ORDER> {
    template <typename TFunctor>
    static void iterate(Array<Quantity>& qs, TFunctor&& functor) {
        for (auto& q : qs) {
            if (q.getTemporalEnum() != TemporalEnum::SECOND_ORDER) {
                continue;
            }
            auto scalar = q.cast<Float, TemporalEnum::SECOND_ORDER>();
            if (scalar) {
                functor(scalar->getValue(), scalar->getDerivative(), scalar->get2ndDerivative());
            } else {
                auto vector = q.cast<Vector, TemporalEnum::SECOND_ORDER>();
                ASSERT(vector);
                functor(vector->getValue(), vector->getDerivative(), vector->get2ndDerivative());
            }
        }
    }

    template <typename TFunctor>
    static void iteratePair(Array<Quantity>& qs1, Array<Quantity>& qs2, TFunctor&& functor) {
        ASSERT(qs1.size() == qs2.size());
        for (int i = 0; i < qs1.size(); ++i) {
            if (qs1[i].getTemporalEnum() != TemporalEnum::SECOND_ORDER) {
                continue;
            }
            ASSERT(qs2[i].getTemporalEnum() == TemporalEnum::SECOND_ORDER);
            auto scalar1 = qs1[i].cast<Float, TemporalEnum::SECOND_ORDER>();
            auto scalar2 = qs2[i].cast<Float, TemporalEnum::SECOND_ORDER>();
            if (scalar1 && scalar2) {
                functor(scalar1->getValue(),
                        scalar1->getDerivative(),
                        scalar1->get2ndDerivative(),
                        scalar2->getValue(),
                        scalar2->getDerivative(),
                        scalar2->get2ndDerivative());
            } else {
                auto vector1 = qs1[i].cast<Vector, TemporalEnum::SECOND_ORDER>();
                auto vector2 = qs2[i].cast<Vector, TemporalEnum::SECOND_ORDER>();
                ASSERT(vector1 && vector2);
                functor(vector1->getValue(),
                        vector1->getDerivative(),
                        vector1->get2ndDerivative(),
                        vector2->getValue(),
                        vector2->getDerivative(),
                        vector2->get2ndDerivative());
            }
        }
    }
};

/*
template <>
struct StorageIterator<TemporalEnum::HIGHEST_ORDER> {
    template <typename TFunctor>
    static void iterate(Array<Quantity>& qs, TFunctor&& functor) {
        for (auto& q : qs) {
            auto scalar = q.cast<Float, TemporalEnum::FIRST_ORDER>();
            if (scalar) {
                functor(scalar->getValue(), scalar->getDerivative());
            } else {
                auto vector = q.cast<Vector, TemporalEnum::FIRST_ORDER>();
                ASSERT(vector);
                functor(vector->getValue(), vector->getDerivative());
            }
        }
    }

    template <typename TFunctor>
    static void iteratePair(Array<Quantity>& qs1, Array<Quantity>& qs2, TFunctor&& functor) {
        ASSERT(qs1.size() == qs2.size());
        for (int i = 0; i < qs1.size(); ++i) {
            if (qs1[i].getTemporalEnum() != TemporalEnum::FIRST_ORDER) {
                continue;
            }
            ASSERT(qs2[i].getTemporalEnum() == TemporalEnum::FIRST_ORDER);
            auto scalar1 = qs1[i].cast<Float, TemporalEnum::FIRST_ORDER>();
            auto scalar2 = qs2[i].cast<Float, TemporalEnum::FIRST_ORDER>();
            if (scalar1 && scalar2) {
                functor(scalar1->getValue(),
                        scalar1->getDerivative(),
                        scalar2->getValue(),
                        scalar2->getDerivative());
            } else {
                auto vector1 = qs1[i].cast<Vector, TemporalEnum::FIRST_ORDER>();
                auto vector2 = qs2[i].cast<Vector, TemporalEnum::FIRST_ORDER>();
                ASSERT(vector1 && vector2);
                functor(vector1->getValue(),
                        vector1->getDerivative(),
                        vector2->getValue(),
                        vector2->getDerivative());
            }
        }
    }
};*/


/// Iterate over given type of quantities and executes functor for each. The functor is used for scalars,
/// vectors and tensors, so it must be a generic lambda or a class with overloaded operator() for each
/// value type.
template <TemporalEnum Type, typename TFunctor>
void iterate(Array<Quantity>& qs, TFunctor&& functor) {
    StorageIterator<Type>::iterate(qs, std::forward<TFunctor>(functor));
}


/// Iterate over given type of quantities in two storage views and executes functor for each pair.
template <TemporalEnum Type, typename TFunctor>
void iteratePair(Array<Quantity>& qs1, Array<Quantity>& qs2, TFunctor&& functor) {
    StorageIterator<Type>::iteratePair(qs1, qs2, std::forward<TFunctor>(functor));
}

NAMESPACE_SPH_END
