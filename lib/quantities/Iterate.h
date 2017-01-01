#pragma once

#include "quantities/Quantity.h"
#include <map>

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
    void visit(Quantity& q, TFunctor&& functor) {
        functor(q.getValue<TValue>());
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
    void visit(Quantity& q, TFunctor&& functor) {
        for (auto& i : q.getBuffers<TValue>()) {
            functor(i);
        }
    }
};
template <typename TFunctor>
struct StoragePairVisitor<VisitorEnum::ALL_BUFFERS, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q1, Quantity& q2, TFunctor&& functor) {
        auto values1 = q1.template getBuffers<TValue>();
        auto values2 = q2.template getBuffers<TValue>();
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
    void visit(Quantity& q, TFunctor&& functor) {
        if (q.getOrderEnum() != OrderEnum::ZERO_ORDER) {
            return;
        }
        functor(q.getValue<TValue>());
    }
};
template <typename TFunctor>
struct StoragePairVisitor<VisitorEnum::ZERO_ORDER, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q1, Quantity& q2, TFunctor&& functor) {
        if (q1.getOrderEnum() != OrderEnum::ZERO_ORDER) {
            return;
        }
        ASSERT(q2.getOrderEnum() == OrderEnum::ZERO_ORDER);
        functor(q1.getValue<TValue>(), q2.getValue<TValue>());
    }
};


/// Iterator over all first-order quantities. This won't access second-order quantities! The functor is
/// executed with two parameters: values and derivatives (both LimitedArrays).
template <typename TFunctor>
struct StorageVisitor<VisitorEnum::FIRST_ORDER, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q, TFunctor&& functor) {
        if (q.getOrderEnum() != OrderEnum::FIRST_ORDER) {
            return;
        }
        /// \todo no dynamic_cast necessary here, maybe use two versions of cast, safe/checked and
        /// unsafe/unchecked
        functor(q.getValue<TValue>(), q.getDt<TValue>());
    }
};
template <typename TFunctor>
struct StoragePairVisitor<VisitorEnum::FIRST_ORDER, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q1, Quantity& q2, TFunctor&& functor) {
        if (q1.getOrderEnum() != OrderEnum::FIRST_ORDER) {
            return;
        }
        ASSERT(q2.getOrderEnum() == OrderEnum::FIRST_ORDER);
        functor(q1.getValue<TValue>(), q1.getDt<TValue>(), q2.getValue<TValue>(), q2.getDt<TValue>());
    }
};

/// Iterator over all second-order quantities. Functor is executed with three parameters: values, 1st
/// derivatives and 2nd derivatives (as LimitedArrays).
template <typename TFunctor>
struct StorageVisitor<VisitorEnum::SECOND_ORDER, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q, TFunctor&& functor) {
        if (q.getOrderEnum() != OrderEnum::SECOND_ORDER) {
            return;
        }
        functor(q.getValue<TValue>(), q.getDt<TValue>(), q.getD2t<TValue>());
    }
};
template <typename TFunctor>
struct StoragePairVisitor<VisitorEnum::SECOND_ORDER, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q1, Quantity& q2, TFunctor&& functor) {
        if (q1.getOrderEnum() != OrderEnum::SECOND_ORDER) {
            return;
        }
        ASSERT(q2.getOrderEnum() == OrderEnum::SECOND_ORDER);
        functor(q1.getValue<TValue>(),
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
    void visit(Quantity& q, TFunctor&& functor) {
        const OrderEnum order = q.getOrderEnum();
        if (order == OrderEnum::FIRST_ORDER) {
            functor(q.getDt<TValue>());
        } else if (order == OrderEnum::SECOND_ORDER) {
            functor(q.getD2t<TValue>());
        }
    }
};
template <typename TFunctor>
struct StoragePairVisitor<VisitorEnum::HIGHEST_DERIVATIVES, TFunctor> {
    template <typename TValue>
    void visit(Quantity& q1, Quantity& q2, TFunctor&& functor) {
        const OrderEnum order1 = q1.getOrderEnum();
        ASSERT(order1 == q2.getOrderEnum());
        if (order1 == OrderEnum::FIRST_ORDER) {
            functor(q1.getDt<TValue>(), q2.getDt<TValue>());
        } else if (order1 == OrderEnum::SECOND_ORDER) {
            functor(q1.getD2t<TValue>(), q2.getD2t<TValue>());
        }
    }
};


/// Iterate over given type of quantities and executes functor for each. The functor is used for scalars,
/// vectors and tensors, so it must be a generic lambda or a class with overloaded operator() for each
/// value type.
template <VisitorEnum Type, typename TFunctor>
void iterate(std::map<QuantityKey, Quantity>& qs, TFunctor&& functor) {
    StorageVisitor<Type, TFunctor> visitor;
    for (auto& q : qs) {
        dispatch(q.second.getValueEnum(), visitor, q.second, std::forward<TFunctor>(functor));
    }
}

template <typename TFunctor>
struct StorageVisitorWithPositions {
    template <typename TValue>
    void visit(Quantity& q, Array<Vector>& r, QuantityKey key, TFunctor&& functor) {
        if (key == QuantityKey::POSITIONS) {
            // exclude positions
            auto buffers = q.getBuffers<TValue>();
            functor(buffers[1], r);
            functor(buffers[2], r);
        } else {
            for (auto& i : q.getBuffers<TValue>()) {
                functor(i, r);
            }
        }
    }
};

/// Iterate over all quantities and execute a functor, passing quantity buffers together with particle
/// positions as arguments. Storage must already contain positions, checked by assert.
template <typename TFunctor>
void iterateWithPositions(std::map<QuantityKey, Quantity>& qs, TFunctor&& functor) {
    auto iter = qs.find(QuantityKey::POSITIONS);
    ASSERT(iter != qs.end());
    Array<Vector>& r = iter->second.getValue<Vector>();
    StorageVisitorWithPositions<TFunctor> visitor;
    for (auto& q : qs) {
        dispatch(q.second.getValueEnum(), visitor, q.second, r, q.first, std::forward<TFunctor>(functor));
    }
}

/// Iterate over selected set of quantities. All quantities in the set must be stored, checked by assert. Can
/// be used with any visitors to further constrain the set of quantities/buffers passed into functor.
template <VisitorEnum Type, typename TFunctor>
void iterateCustom(std::map<QuantityKey, Quantity>& qs,
    Array<QuantityKey>&& set,
    TFunctor&& functor) {
    StorageVisitor<Type, TFunctor> visitor;
    for (QuantityKey key : set) {
        auto iter = qs.find(key);
        ASSERT(iter != qs.end());
        dispatch(iter->second.getValueEnum(), visitor, iter->second, std::forward<TFunctor>(functor));
    }
}


/// Iterate over given type of quantities in two storage views and executes functor for each pair.
template <VisitorEnum Type, typename TFunctor>
void iteratePair(std::map<QuantityKey, Quantity>& qs1,
    std::map<QuantityKey, Quantity>& qs2,
    TFunctor&& functor) {
    ASSERT(qs1.size() == qs2.size());
    StoragePairVisitor<Type, TFunctor> visitor;
    for (auto i1 = qs1.begin(), i2 = qs2.begin(); i1 != qs1.end(); ++i1, ++i2) {
        Quantity& q1 = i1->second;
        Quantity& q2 = i2->second;
        ASSERT(q1.getValueEnum() == q2.getValueEnum());
        dispatch(q1.getValueEnum(), visitor, q1, q2, std::forward<TFunctor>(functor));
    }
}

NAMESPACE_SPH_END
