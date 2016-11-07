#pragma once

#include "storage/Quantity.h"

NAMESPACE_SPH_BEGIN


enum class IterateType {
    FIRST_ORDER,  ///< Quantities evolved in time using its first derivatives
    SECOND_ORDER, ///< Quantities evolved in time using its first and second derivatives derivatives
    ALL,
};


template <IterateType Type>
struct StorageIterator;

template <>
struct StorageIterator<IterateType::ALL> {
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
struct StorageIterator<IterateType::FIRST_ORDER> {
    template <typename TFunctor>
    static void iterate(Array<Quantity>& qs, TFunctor&& functor) {
        for (auto& q : qs) {
            auto scalar = q.cast<Float, TemporalType::FIRST_ORDER>();
            ASSERT(scalar);
            functor(scalar->getValue(), scalar->getDerivative());
            auto vector = q.cast<Vector, TemporalType::FIRST_ORDER>();
            ASSERT(vector);
            functor(vector->getValue(), vector->getDerivative());
        }
    }

    template <typename TFunctor>
    static void iteratePair(Array<Quantity>& qs1, Array<Quantity>& qs2, TFunctor&& functor) {
        ASSERT(qs1.size() == qs2.size());
        for (int i = 0; i < qs1.size(); ++i) {
            auto scalar1 = qs1[i].cast<Float, TemporalType::FIRST_ORDER>();
            auto scalar2 = qs2[i].cast<Float, TemporalType::FIRST_ORDER>();
            ASSERT(scalar1 && scalar2);
            functor(scalar1->getValue(),
                    scalar1->getDerivative(),
                    scalar2->getValue(),
                    scalar2->getDerivative());
            auto vector1 = qs1[i].cast<Vector, TemporalType::FIRST_ORDER>();
            auto vector2 = qs2[i].cast<Vector, TemporalType::FIRST_ORDER>();
            ASSERT(vector1 && vector2);
            functor(vector1->getValue(),
                    vector1->getDerivative(),
                    vector2->getValue(),
                    vector2->getDerivative());
        }
    }
};

template <>
struct StorageIterator<IterateType::SECOND_ORDER> {
    template <typename TFunctor>
    static void iterate(Array<Quantity>& qs, TFunctor&& functor) {
        for (auto& q : qs) {
            auto scalar = q.cast<Float, TemporalType::SECOND_ORDER>();
            ASSERT(scalar);
            functor(scalar->getValue(), scalar->getDerivative(), scalar->get2ndDerivative());
            auto vector = q.cast<Vector, TemporalType::SECOND_ORDER>();
            ASSERT(vector);
            functor(vector->getValue(), vector->getDerivative(), vector->get2ndDerivative());
        }
    }

    template <typename TFunctor>
    static void iteratePair(Array<Quantity>& qs1, Array<Quantity>& qs2, TFunctor&& functor) {
        ASSERT(qs1.size() == qs2.size());
        for (int i = 0; i < qs1.size(); ++i) {
            auto scalar1 = qs1[i].cast<Float, TemporalType::SECOND_ORDER>();
            auto scalar2 = qs2[i].cast<Float, TemporalType::SECOND_ORDER>();
            ASSERT(scalar1 && scalar2);
            functor(scalar1->getValue(),
                    scalar1->getDerivative(),
                    scalar1->get2ndDerivative(),
                    scalar2->getValue(),
                    scalar2->getDerivative(),
                    scalar2->get2ndDerivative());
            auto vector1 = qs1[i].cast<Vector, TemporalType::SECOND_ORDER>();
            auto vector2 = qs2[i].cast<Vector, TemporalType::SECOND_ORDER>();
            ASSERT(vector1 && vector2);
            functor(vector1->getValue(),
                    vector1->getDerivative(),
                    vector1->get2ndDerivative(),
                    vector2->getValue(),
                    vector2->getDerivative(),
                    vector2->get2ndDerivative());
        }
    }
};


/// Iterate over given type of quantities and executes functor for each. The functor is used for scalars,
/// vectors and tensors, so it must be a generic lambda or a class with overloaded operator() for each
/// value type.
template <IterateType Type, typename TFunctor>
void iterate(Array<Quantity>& qs, TFunctor&& functor) {
    StorageIterator<Type>::iterate(qs, std::forward<TFunctor>(functor));
}


/// Iterate over given type of quantities in two storage views and executes functor for each pair.
template <IterateType Type, typename TFunctor>
void iteratePair(Array<Quantity>& qs1, Array<Quantity>& qs2, TFunctor&& functor) {
    StorageIterator<Type>::iteratePair(qs1, qs2,
                                       std::forward<TFunctor>(functor));
}

NAMESPACE_SPH_END
