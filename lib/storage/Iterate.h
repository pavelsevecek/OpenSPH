#pragma once

#include "storage/Quantity.h"

NAMESPACE_SPH_BEGIN

/// Helper structure allowing to iterate over values/derivatives of quantities in storage. The type of
/// iteration if selected by template parameter Type; struct need to be specialized for all options and each
/// one must implement static methods iterate() that iterates over quantities of one storage, and
/// iteratePair() that iterates over quantities of two storages at the same time (assuming the storages
/// contain the same quantities).
///
/// \note We have to pass TFunctor type here as visit<> method must have only one argument (type).
template <TemporalEnum Type, typename TFunctor>
struct StorageVisitor;

template <TemporalEnum Type, typename TFunctor>
struct StoragePairVisitor;


/// Iterator over all buffers. This will execute given functor for each buffer of each quantities, meaning
/// both values of the quantities and all derivatives. Stored buffers are passed as LimitedArray to the
/// functor. Useful for operations such as merging two storages, resizing all arrays of the storage, ...
template <typename TFunctor>
struct StorageVisitor<TemporalEnum::ALL, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q, TFunctor&& functor) {
        for (auto& i : q.template getBuffers<TValue>()) {
            functor(i);
        }
    }
};
template <typename TFunctor>
struct StoragePairVisitor<TemporalEnum::ALL, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q1, Quantity& q2, TFunctor&& functor) {
        auto values1 = q1.template getBuffers<TValue>();
        auto values2 = q2.template getBuffers<TValue>();
        ASSERT(values1.size() == values2.size());
        for (int j = 0; j < values1.size(); ++j) {
            functor(values1[j], values2[j]);
        }
    }
};

/// Iterator over all first-order quantities. This won't access second-order quantities! The functor is
/// executed with two parameters: values and derivatives (both LimitedArrays).
template <typename TFunctor>
struct StorageVisitor<TemporalEnum::FIRST_ORDER, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q, TFunctor&& functor) {
        if (q.getTemporalEnum() != TemporalEnum::FIRST_ORDER) {
            return;
        }
        /// \todo no dynamic_cast necessary here, maybe use two versions of cast, safe/checked and
        /// unsafe/unchecked
        auto holder = q.cast<TValue, TemporalEnum::FIRST_ORDER>();
        ASSERT(holder);
        functor(holder->getValue(), holder->getDerivative());
    }
};

template <typename TFunctor>
struct StoragePairVisitor<TemporalEnum::FIRST_ORDER, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q1, Quantity& q2, TFunctor&& functor) {
        if (q1.getTemporalEnum() != TemporalEnum::FIRST_ORDER) {
            return;
        }
        ASSERT(q2.getTemporalEnum() == TemporalEnum::FIRST_ORDER);
        auto holder1 = q1.cast<TValue, TemporalEnum::FIRST_ORDER>();
        auto holder2 = q2.cast<TValue, TemporalEnum::FIRST_ORDER>();
        ASSERT(holder1 && holder2);
        functor(holder1->getValue(), holder1->getDerivative(), holder2->getValue(), holder2->getDerivative());
    }
};

/// Iterator over all second-order quantities. Functor is executed with three parameters: values, 1st
/// derivatives and 2nd derivatives (as LimitedArrays).
template <typename TFunctor>
struct StorageVisitor<TemporalEnum::SECOND_ORDER, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q, TFunctor&& functor) {
        if (q.getTemporalEnum() != TemporalEnum::SECOND_ORDER) {
            return;
        }
        auto holder = q.cast<TValue, TemporalEnum::SECOND_ORDER>();
        ASSERT(holder);
        functor(holder->getValue(), holder->getDerivative(), holder->get2ndDerivative());
    }
};

template <typename TFunctor>
struct StoragePairVisitor<TemporalEnum::SECOND_ORDER, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q1, Quantity& q2, TFunctor&& functor) {
        if (q1.getTemporalEnum() != TemporalEnum::SECOND_ORDER) {
            return;
        }
        ASSERT(q2.getTemporalEnum() == TemporalEnum::SECOND_ORDER);
        auto holder1 = q1.cast<TValue, TemporalEnum::SECOND_ORDER>();
        auto holder2 = q2.cast<TValue, TemporalEnum::SECOND_ORDER>();
        ASSERT(holder1 && holder2);
        functor(holder1->getValue(),
                holder1->getDerivative(),
                holder1->get2ndDerivative(),
                holder2->getValue(),
                holder2->getDerivative(),
                holder2->get2ndDerivative());
    }
};


/// Iterate over given type of quantities and executes functor for each. The functor is used for scalars,
/// vectors and tensors, so it must be a generic lambda or a class with overloaded operator() for each
/// value type.
template <TemporalEnum Type, typename TFunctor>
void iterate(Array<Quantity>& qs, TFunctor&& functor) {
    StorageVisitor<Type, TFunctor> visitor;
    for (auto& q : qs) {
        dispatch(q.getValueEnum(), visitor, q, std::forward<TFunctor>(functor));
    }
}


/// Iterate over given type of quantities in two storage views and executes functor for each pair.
template <TemporalEnum Type, typename TFunctor>
void iteratePair(Array<Quantity>& qs1, Array<Quantity>& qs2, TFunctor&& functor) {
    ASSERT(qs1.size() == qs2.size());
    StoragePairVisitor<Type, TFunctor> visitor;
    for (int i = 0; i < qs1.size(); ++i) {
        ASSERT(qs1[i].getValueEnum() == qs2[i].getValueEnum());
        dispatch(qs1[i].getValueEnum(), visitor, qs1[i], qs2[i], std::forward<TFunctor>(functor));
    }
}

NAMESPACE_SPH_END
