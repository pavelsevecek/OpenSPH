#pragma once

/// \file Iterate.h
/// \brief Functions for iterating over individual quantities in Storage
/// \author Pavel Sevecek
/// \date 2016-2021

#include "objects/utility/IteratorAdapters.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

/// \section Visitors of storage quantities
///
/// Helper structure allowing to iterate over values/derivatives of quantities in storage. The type of
/// iteration if selected by template parameter Type; struct need to be specialized for all options and each
/// one must implement static methods iterate() that iterates over quantities of one storage, and
/// iteratePair() that iterates over quantities of two storages at the same time (assuming the storages
/// contain the same quantities).
template <VisitorEnum TVisitorType>
struct StorageVisitor;

template <VisitorEnum TVisitorType>
struct StoragePairVisitor;


/// \brief Iterator over all quantities, executes given functor with buffer of quantity values.
///
/// Does not access derivatives of quantities. Stored buffers are passed as Array to the functor.
template <>
struct StorageVisitor<VisitorEnum::ALL_VALUES> {
    template <typename TValue, typename TFunctor>
    void visit(Quantity& q, const QuantityId id, TFunctor&& functor) {
        functor(id, q.getValue<TValue>());
    }
};
template <>
struct StoragePairVisitor<VisitorEnum::ALL_VALUES> {
    template <typename TValue, typename TFunctor>
    void visit(Quantity& q1, Quantity& q2, TFunctor&& functor) {
        functor(q1.getValue<TValue>(), q2.getValue<TValue>());
    }
};

/// \brief Iterator over all buffers.
///
/// This will execute given functor for each buffer of each quantities, meaning both values of the quantities
/// and all derivatives. Stored buffers are passed as Array to the functor. Useful for operations such
/// as merging two storages, resizing all arrays of the storage, ...
template <>
struct StorageVisitor<VisitorEnum::ALL_BUFFERS> {
    template <typename TValue, typename TFunctor>
    void visit(Quantity& q, const QuantityId UNUSED(id), TFunctor&& functor) {
        for (auto& i : q.getAll<TValue>()) {
            functor(i);
        }
    }
    template <typename TValue, typename TFunctor>
    void visit(const Quantity& q, const QuantityId UNUSED(id), TFunctor&& functor) {
        for (const auto& i : q.getAll<TValue>()) {
            functor(i);
        }
    }
};
template <>
struct StoragePairVisitor<VisitorEnum::ALL_BUFFERS> {
    template <typename TValue, typename TFunctor>
    void visit(Quantity& q1, Quantity& q2, const QuantityId UNUSED(id), TFunctor&& functor) {
        auto values1 = q1.template getAll<TValue>();
        auto values2 = q2.template getAll<TValue>();
        SPH_ASSERT(values1.size() == values2.size());
        for (Size j = 0; j < values1.size(); ++j) {
            functor(values1[j], values2[j]);
        }
    }
};


/// \brief Iterator over all zero-order quantities.
///
/// This won't access first-order quantities nor second-order quantities! The functor is executed with array
/// of quantity values.
template <>
struct StorageVisitor<VisitorEnum::ZERO_ORDER> {
    template <typename TValue, typename TFunctor>
    void visit(Quantity& q, const QuantityId id, TFunctor&& functor) {
        if (q.getOrderEnum() != OrderEnum::ZERO) {
            return;
        }
        functor(id, q.getValue<TValue>());
    }
    template <typename TValue, typename TFunctor>
    void visit(const Quantity& q, const QuantityId id, TFunctor&& functor) {
        if (q.getOrderEnum() != OrderEnum::ZERO) {
            return;
        }
        functor(id, q.getValue<TValue>());
    }
};
template <>
struct StoragePairVisitor<VisitorEnum::ZERO_ORDER> {
    template <typename TValue, typename TFunctor>
    void visit(Quantity& q1, Quantity& q2, const QuantityId id, TFunctor&& functor) {
        if (q1.getOrderEnum() != OrderEnum::ZERO) {
            return;
        }
        SPH_ASSERT(q2.getOrderEnum() == OrderEnum::ZERO);
        functor(id, q1.getValue<TValue>(), q2.getValue<TValue>());
    }

    template <typename TValue, typename TFunctor>
    void visit(const Quantity& q1, const Quantity& q2, const QuantityId id, TFunctor&& functor) {
        if (q1.getOrderEnum() != OrderEnum::ZERO) {
            return;
        }
        SPH_ASSERT(q2.getOrderEnum() == OrderEnum::ZERO);
        functor(id, q1.getValue<TValue>(), q2.getValue<TValue>());
    }
};


/// \brief Iterator over all first-order quantities.
///
/// This won't access second-order quantities! The functor is executed with two parameters: values and
/// derivatives (both Arrays).
template <>
struct StorageVisitor<VisitorEnum::FIRST_ORDER> {
    template <typename TValue, typename TFunctor>
    void visit(Quantity& q, const QuantityId id, TFunctor&& functor) {
        if (q.getOrderEnum() != OrderEnum::FIRST) {
            return;
        }
        functor(id, q.getValue<TValue>(), q.getDt<TValue>());
    }
    template <typename TValue, typename TFunctor>
    void visit(const Quantity& q, const QuantityId id, TFunctor&& functor) {
        if (q.getOrderEnum() != OrderEnum::FIRST) {
            return;
        }
        functor(id, q.getValue<TValue>(), q.getDt<TValue>());
    }
};
template <>
struct StoragePairVisitor<VisitorEnum::FIRST_ORDER> {
    template <typename TValue, typename TFunctor>
    void visit(Quantity& q1, Quantity& q2, const QuantityId id, TFunctor&& functor) {
        if (q1.getOrderEnum() != OrderEnum::FIRST) {
            return;
        }
        SPH_ASSERT(q2.getOrderEnum() == OrderEnum::FIRST);
        functor(id, q1.getValue<TValue>(), q1.getDt<TValue>(), q2.getValue<TValue>(), q2.getDt<TValue>());
    }

    template <typename TValue, typename TFunctor>
    void visit(const Quantity& q1, const Quantity& q2, const QuantityId id, TFunctor&& functor) {
        if (q1.getOrderEnum() != OrderEnum::FIRST) {
            return;
        }
        SPH_ASSERT(q2.getOrderEnum() == OrderEnum::FIRST);
        functor(id, q1.getValue<TValue>(), q1.getDt<TValue>(), q2.getValue<TValue>(), q2.getDt<TValue>());
    }
};

/// Iterator over all second-order quantities. Functor is executed with three parameters: values, 1st
/// derivatives and 2nd derivatives (as Array).
template <>
struct StorageVisitor<VisitorEnum::SECOND_ORDER> {
    template <typename TValue, typename TFunctor>
    void visit(Quantity& q, const QuantityId id, TFunctor&& functor) {
        if (q.getOrderEnum() != OrderEnum::SECOND) {
            return;
        }
        functor(id, q.getValue<TValue>(), q.getDt<TValue>(), q.getD2t<TValue>());
    }
    template <typename TValue, typename TFunctor>
    void visit(const Quantity& q, const QuantityId id, TFunctor&& functor) {
        if (q.getOrderEnum() != OrderEnum::SECOND) {
            return;
        }
        functor(id, q.getValue<TValue>(), q.getDt<TValue>(), q.getD2t<TValue>());
    }
};
template <>
struct StoragePairVisitor<VisitorEnum::SECOND_ORDER> {
    template <typename TValue, typename TFunctor>
    void visit(Quantity& q1, Quantity& q2, const QuantityId id, TFunctor&& functor) {
        if (q1.getOrderEnum() != OrderEnum::SECOND) {
            return;
        }
        SPH_ASSERT(q2.getOrderEnum() == OrderEnum::SECOND);
        functor(id,
            q1.getValue<TValue>(),
            q1.getDt<TValue>(),
            q1.getD2t<TValue>(),
            q2.getValue<TValue>(),
            q2.getDt<TValue>(),
            q2.getD2t<TValue>());
    }

    template <typename TValue, typename TFunctor>
    void visit(const Quantity& q1, const Quantity& q2, const QuantityId id, TFunctor&& functor) {
        if (q1.getOrderEnum() != OrderEnum::SECOND) {
            return;
        }
        SPH_ASSERT(q2.getOrderEnum() == OrderEnum::SECOND);
        functor(id,
            q1.getValue<TValue>(),
            q1.getDt<TValue>(),
            q1.getD2t<TValue>(),
            q2.getValue<TValue>(),
            q2.getDt<TValue>(),
            q2.getD2t<TValue>());
    }
};


/// \brief Iterator over all highest-order derivatives of quantities.
///
/// This won't access zero-order quantities. Passes single array to the functor; second derivatives for
/// second-order quantities and first derivatives for first-order quantitites.
template <>
struct StorageVisitor<VisitorEnum::HIGHEST_DERIVATIVES> {
    template <typename TValue, typename TFunctor>
    void visit(Quantity& q, const QuantityId id, TFunctor&& functor) {
        const OrderEnum order = q.getOrderEnum();
        if (order == OrderEnum::FIRST) {
            functor(id, q.getDt<TValue>());
        } else if (order == OrderEnum::SECOND) {
            functor(id, q.getD2t<TValue>());
        }
    }
    template <typename TValue, typename TFunctor>
    void visit(const Quantity& q, const QuantityId id, TFunctor&& functor) {
        const OrderEnum order = q.getOrderEnum();
        if (order == OrderEnum::FIRST) {
            functor(id, q.getDt<TValue>());
        } else if (order == OrderEnum::SECOND) {
            functor(id, q.getD2t<TValue>());
        }
    }
};
template <>
struct StoragePairVisitor<VisitorEnum::HIGHEST_DERIVATIVES> {
    template <typename TValue, typename TFunctor>
    void visit(Quantity& q1, Quantity& q2, TFunctor&& functor) {
        const OrderEnum order1 = q1.getOrderEnum();
        SPH_ASSERT(order1 == q2.getOrderEnum());
        if (order1 == OrderEnum::FIRST) {
            functor(q1.getDt<TValue>(), q2.getDt<TValue>());
        } else if (order1 == OrderEnum::SECOND) {
            functor(q1.getD2t<TValue>(), q2.getD2t<TValue>());
        }
    }
};


/// \brief Iterate over given type of quantities and executes functor for each.
///
/// The functor is used for scalars, vectors and tensors, so it must be a generic lambda or a class with
/// overloaded operator() for each value type.
/// \tparam Type Subset of visited quantities, also defines parameters of the functor, see VisitorEnum.
/// \param storage Storage containing visited quantities
/// \param scheduler Scheduler used to parallelize processing of quantities
/// \param functor Functor executed for every quantity or buffer matching the specified criterion
template <VisitorEnum Type, typename TFunctor>
void iterate(Storage& storage, IScheduler& scheduler, TFunctor&& functor) {
    StorageVisitor<Type> visitor;
    StorageSequence sequence = storage.getQuantities();
    parallelFor(scheduler, 0, sequence.size(), 1, [&](const Size i) {
        StorageElement q = sequence[i];
        dispatch(q.quantity.getValueEnum(), visitor, q.quantity, q.id, std::forward<TFunctor>(functor));
    });
}

template <VisitorEnum Type, typename TFunctor>
void iterate(Storage& storage, TFunctor&& functor) {
    iterate<Type>(storage, SEQUENTIAL, std::forward<TFunctor>(functor));
}

/// \copydoc iterate
template <VisitorEnum Type, typename TFunctor>
void iterate(const Storage& storage, IScheduler& scheduler, TFunctor&& functor) {
    StorageVisitor<Type> visitor;
    ConstStorageSequence sequence = storage.getQuantities();
    parallelFor(scheduler, 0, sequence.size(), 1, [&](const Size i) {
        ConstStorageElement q = sequence[i];
        dispatch(q.quantity.getValueEnum(), visitor, q.quantity, q.id, std::forward<TFunctor>(functor));
    });
}

template <VisitorEnum Type, typename TFunctor>
void iterate(const Storage& storage, TFunctor&& functor) {
    iterate<Type>(storage, SEQUENTIAL, std::forward<TFunctor>(functor));
}

struct StorageVisitorWithPositions {
    template <typename TValue, typename TFunctor>
    void visit(Quantity& q, Array<Vector>& r, QuantityId key, TFunctor&& functor) {
        if (key == QuantityId::POSITION) {
            // exclude positions, iterate only derivatives
            auto buffers = q.getAll<TValue>();
            // usually, there are 3 buffers (positions, velocities, accelerations), but we allow any number of
            // buffers to make it more general (and this is actually used in DiehlDistribution, for example).
            for (Size i = 1; i < buffers.size(); ++i) {
                functor(buffers[i], r);
            }
        } else {
            for (auto& i : q.getAll<TValue>()) {
                functor(i, r);
            }
        }
    }
};

/// \brief Iterate over all quantities and execute a functor, passing quantity buffers together with particle
/// positions as arguments.
///
/// Storage must already contain positions, checked by assert.
template <typename TFunctor>
void iterateWithPositions(Storage& storage, TFunctor&& functor) {
    Array<Vector>& r = storage.getValue<Vector>(QuantityId::POSITION);
    StorageVisitorWithPositions visitor;
    for (auto q : storage.getQuantities()) {
        dispatch(q.quantity.getValueEnum(), visitor, q.quantity, r, q.id, functor);
    }
}

/// Iterate over given type of quantities in two storage views and executes functor for each pair.
template <VisitorEnum Type, typename TFunctor>
void iteratePair(Storage& storage1, Storage& storage2, TFunctor&& functor) {
    SPH_ASSERT(storage1.getQuantityCnt() == storage2.getQuantityCnt(),
        storage1.getQuantityCnt(),
        storage2.getQuantityCnt());
    StoragePairVisitor<Type> visitor;
    struct Element {
        StorageElement e1;
        StorageElement e2;
    };

    for (auto i : iterateTuple<Element>(storage1.getQuantities(), storage2.getQuantities())) {
        SPH_ASSERT(i.e1.id == i.e2.id);
        Quantity& q1 = i.e1.quantity;
        Quantity& q2 = i.e2.quantity;
        SPH_ASSERT(q1.getValueEnum() == q2.getValueEnum());
        dispatch(q1.getValueEnum(), visitor, q1, q2, i.e1.id, functor);
    }
}

/// \copydoc iteratePair
template <VisitorEnum Type, typename TFunctor>
void iteratePair(const Storage& storage1, const Storage& storage2, TFunctor&& functor) {
    SPH_ASSERT(storage1.getQuantityCnt() == storage2.getQuantityCnt(),
        storage1.getQuantityCnt(),
        storage2.getQuantityCnt());
    StoragePairVisitor<Type> visitor;
    struct Element {
        ConstStorageElement e1;
        ConstStorageElement e2;
    };

    for (auto i : iterateTuple<Element>(storage1.getQuantities(), storage2.getQuantities())) {
        SPH_ASSERT(i.e1.id == i.e2.id);
        const Quantity& q1 = i.e1.quantity;
        const Quantity& q2 = i.e2.quantity;
        SPH_ASSERT(q1.getValueEnum() == q2.getValueEnum());
        dispatch(q1.getValueEnum(), visitor, q1, q2, i.e1.id, functor);
    }
}


NAMESPACE_SPH_END
