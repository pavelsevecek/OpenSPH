#pragma once

#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN


/// Helper structure allowing to iterate over values/derivatives of quantities in storage. The type of
/// iteration if selected by template parameter Type; struct need to be specialized for all options and each
/// one must implement static methods iterate() that iterates over quantities of one storage, and
/// iteratePair() that iterates over quantities of two storages at the same time (assuming the storages
/// contain the same quantities).
///
/// \note We have to pass TFunctor type here as visit<> method must have only one argument (type).
template <VisitorEnum TVisitorType, typename TFunctor>
struct StorageVisitor;

template <VisitorEnum TVisitorType, typename TFunctor>
struct StoragePairVisitor;


/// Iterator over all quantities, executes given functor with buffer of quantity values. Does not access
/// derivatives of quantities. Stored buffers are passed as LimitedArray to the functor.
template <typename TFunctor>
struct StorageVisitor<VisitorEnum::ALL_VALUES, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q, const QuantityId id, TFunctor&& functor) {
        functor(id, q.getValue<TValue>());
    }
};
template <typename TFunctor>
struct StoragePairVisitor<VisitorEnum::ALL_VALUES, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q1, Quantity& q2, TFunctor&& functor) {
        functor(q1.getValue<TValue>(), q2.getValue<TValue>());
    }
};

/// Iterator over all buffers. This will execute given functor for each buffer of each quantities, meaning
/// both values of the quantities and all derivatives. Stored buffers are passed as LimitedArray to the
/// functor. Useful for operations such as merging two storages, resizing all arrays of the storage, ...
template <typename TFunctor>
struct StorageVisitor<VisitorEnum::ALL_BUFFERS, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q, const QuantityId UNUSED(id), TFunctor&& functor) {
        for (auto& i : q.getAll<TValue>()) {
            functor(i);
        }
    }
};
template <typename TFunctor>
struct StoragePairVisitor<VisitorEnum::ALL_BUFFERS, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q1, Quantity& q2, const QuantityId UNUSED(id), TFunctor&& functor) {
        auto values1 = q1.template getAll<TValue>();
        auto values2 = q2.template getAll<TValue>();
        ASSERT(values1.size() == values2.size());
        for (Size j = 0; j < values1.size(); ++j) {
            functor(values1[j], values2[j]);
        }
    }
};


/// Iterator over all zero-order quantities. This won't access first-order quantities nor second-order
/// quantities! The functor is executed with array of quantity values.
template <typename TFunctor>
struct StorageVisitor<VisitorEnum::ZERO_ORDER, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q, const QuantityId id, TFunctor&& functor) {
        if (q.getOrderEnum() != OrderEnum::ZERO) {
            return;
        }
        functor(id, q.getValue<TValue>());
    }
};
template <typename TFunctor>
struct StoragePairVisitor<VisitorEnum::ZERO_ORDER, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q1, Quantity& q2, const QuantityId id, TFunctor&& functor) {
        if (q1.getOrderEnum() != OrderEnum::ZERO) {
            return;
        }
        ASSERT(q2.getOrderEnum() == OrderEnum::ZERO);
        functor(id, q1.getValue<TValue>(), q2.getValue<TValue>());
    }
};


/// Iterator over all first-order quantities. This won't access second-order quantities! The functor is
/// executed with two parameters: values and derivatives (both LimitedArrays).
template <typename TFunctor>
struct StorageVisitor<VisitorEnum::FIRST_ORDER, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q, const QuantityId id, TFunctor&& functor) {
        if (q.getOrderEnum() != OrderEnum::FIRST) {
            return;
        }
        functor(id, q.getValue<TValue>(), q.getDt<TValue>());
    }
};
template <typename TFunctor>
struct StoragePairVisitor<VisitorEnum::FIRST_ORDER, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q1, Quantity& q2, const QuantityId id, TFunctor&& functor) {
        if (q1.getOrderEnum() != OrderEnum::FIRST) {
            return;
        }
        ASSERT(q2.getOrderEnum() == OrderEnum::FIRST);
        functor(id, q1.getValue<TValue>(), q1.getDt<TValue>(), q2.getValue<TValue>(), q2.getDt<TValue>());
    }
};

/// Iterator over all second-order quantities. Functor is executed with three parameters: values, 1st
/// derivatives and 2nd derivatives (as LimitedArrays).
template <typename TFunctor>
struct StorageVisitor<VisitorEnum::SECOND_ORDER, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q, const QuantityId id, TFunctor&& functor) {
        if (q.getOrderEnum() != OrderEnum::SECOND) {
            return;
        }
        functor(id, q.getValue<TValue>(), q.getDt<TValue>(), q.getD2t<TValue>());
    }
};
template <typename TFunctor>
struct StoragePairVisitor<VisitorEnum::SECOND_ORDER, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q1, Quantity& q2, const QuantityId id, TFunctor&& functor) {
        if (q1.getOrderEnum() != OrderEnum::SECOND) {
            return;
        }
        ASSERT(q2.getOrderEnum() == OrderEnum::SECOND);
        functor(id,
            q1.getValue<TValue>(),
            q1.getDt<TValue>(),
            q1.getD2t<TValue>(),
            q2.getValue<TValue>(),
            q2.getDt<TValue>(),
            q2.getD2t<TValue>());
    }
};


/// Iterator over all highest-order derivatives of quantities. This won't access zero-order quantities. Passes
/// single array to the functor; second derivatives for second-order quantities and first derivatives for
/// first-order quantitites.
template <typename TFunctor>
struct StorageVisitor<VisitorEnum::HIGHEST_DERIVATIVES, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q, const QuantityId id, TFunctor&& functor) {
        const OrderEnum order = q.getOrderEnum();
        if (order == OrderEnum::FIRST) {
            functor(id, q.getDt<TValue>());
        } else if (order == OrderEnum::SECOND) {
            functor(id, q.getD2t<TValue>());
        }
    }
};
template <typename TFunctor>
struct StoragePairVisitor<VisitorEnum::HIGHEST_DERIVATIVES, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q1, Quantity& q2, TFunctor&& functor) {
        const OrderEnum order1 = q1.getOrderEnum();
        ASSERT(order1 == q2.getOrderEnum());
        if (order1 == OrderEnum::FIRST) {
            functor(q1.getDt<TValue>(), q2.getDt<TValue>());
        } else if (order1 == OrderEnum::SECOND) {
            functor(q1.getD2t<TValue>(), q2.getD2t<TValue>());
        }
    }
};


/// Iterate over given type of quantities and executes functor for each. The functor is used for scalars,
/// vectors and tensors, so it must be a generic lambda or a class with overloaded operator() for each
/// value type.
template <VisitorEnum Type, typename TFunctor>
void iterate(Storage& storage, TFunctor&& functor) {
    StorageVisitor<Type, TFunctor> visitor;
    for (auto q : storage.getQuantities()) {
        dispatch(q.quantity.getValueEnum(), visitor, q.quantity, q.id, std::forward<TFunctor>(functor));
    }
}

template <typename TFunctor>
struct StorageVisitorWithPositions {
    template <typename TValue>
    void visit(Quantity& q, Array<Vector>& r, QuantityId key, TFunctor&& functor) {
        if (key == QuantityId::POSITIONS) {
            // exclude positions
            auto buffers = q.getAll<TValue>();
            functor(buffers[1], r);
            functor(buffers[2], r);
        } else {
            for (auto& i : q.getAll<TValue>()) {
                functor(i, r);
            }
        }
    }
};

/// Iterate over all quantities and execute a functor, passing quantity buffers together with particle
/// positions as arguments. Storage must already contain positions, checked by assert.
template <typename TFunctor>
void iterateWithPositions(Storage& storage, TFunctor&& functor) {
    Array<Vector>& r = storage.getValue<Vector>(QuantityId::POSITIONS);
    StorageVisitorWithPositions<TFunctor> visitor;
    for (auto q : storage.getQuantities()) {
        dispatch(q.quantity.getValueEnum(), visitor, q.quantity, r, q.id, std::forward<TFunctor>(functor));
    }
}

/// Iterate over given type of quantities in two storage views and executes functor for each pair.
template <VisitorEnum Type, typename TFunctor>
void iteratePair(Storage& storage1, Storage& storage2, TFunctor&& functor) {
    ASSERT(storage1.getQuantityCnt() == storage2.getQuantityCnt(), storage1.getQuantityCnt(), storage2.getQuantityCnt());
    StoragePairVisitor<Type, TFunctor> visitor;
    struct Element {
        StorageElement e1;
        StorageElement e2;
    };

    for (auto i : iterateTuple<Element>(storage1.getQuantities(), storage2.getQuantities())) {
        ASSERT(i.e1.id == i.e2.id);
        Quantity& q1 = i.e1.quantity;
        Quantity& q2 = i.e2.quantity;
        ASSERT(q1.getValueEnum() == q2.getValueEnum());
        dispatch(q1.getValueEnum(), visitor, q1, q2, i.e1.id, std::forward<TFunctor>(functor));
    }
}

NAMESPACE_SPH_END
