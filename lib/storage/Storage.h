#pragma once

#include "objects/containers/Array.h"
#include "objects/containers/Tuple.h"
#include "storage/Iterate.h"
#include "storage/QuantityKey.h"
#include "system/Logger.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN


/// Usage:
/// body1 = storage(bodysettings1)
/// body2 = storage(bodysettings2)
/// merged = merge(body1, body2) // now merged contains quantities AND references to body settings. there
///                                 really shouldn't be that many bodies, so copying settings is not a problem
/// continuitysolver(merged)
///   -- merged.emplace(r, P, rho, u, ...)  -- based on bodysettings
///   -- force saves arrayview ...
///
/// notes:
///  you can still edit storage of course, we are holding arrayviews
///  any resizing, adding more bodies, removing particles etc. must be called update(Storage& ... ) method

/// Base object for storing scalar, vector and tensor quantities of SPH particles. Other parts of the code
/// simply point to stored arrays using ArrayView.
class Storage : public Noncopyable {
private:
    Array<Quantity> quantities;
    int particleCnt;

public:
    Storage() = default;

    Storage(Storage&& other)
        : quantities(std::move(other.quantities))
        , particleCnt(other.particleCnt) {}

    Storage& operator=(Storage&& other) {
        quantities  = std::move(other.quantities);
        particleCnt = other.particleCnt;
        return *this;
    }

    /// Creates a quantity in the storage, given its ID, value type and order. Quantity is resized and filled
    /// with default value.
    /// If a quantity with the same ID already exists, it is simply returned unchanged; its type and order,
    /// however, must match the required ones (checked by assert).
    /// \tparam TKey Unique ID of the quantity.
    /// \tparam TValue Type of the quantity. Can be scalar, vector, tensor or traceless tensor.
    /// \tparam TOrder Order (number of derivatives) associated with the quantity.
    /// \param defaultValue Value to which quantity is initialized. If the quantity already exists in the
    ///                     storage, the value is unused.
    /// \param range Optional parameter specifying lower and upper bound of the quantity. Bound are enforced
    ///              by timestepping algorithm. By default, quantities are unbounded.
    /// \return Array of references to LimitedArrays, containing quantity values and derivatives.
    template <QuantityKey TKey, typename TValue, OrderEnum TOrder>
    auto emplace(const TValue& defaultValue, const Optional<Range> range = NOTHING) {
        // linear search in array of quantities
        for (Quantity& q : quantities) {
            if (q.getKey() == TKey) {
                // found the right quantity
                auto optHolder = q.template cast<TValue, QuantityKey>();
                ASSERT(optHolder); // type or order mismatch!
                return optHolder->getBuffers();
            }
        }
        // quantity not found, create it.
        ASSERT(particleCnt != 0);
        Quantity q;
        q.template emplace<TValue, TOrder>(TKey, defaultValue, range);
        quantities.push(std::move(q));
    }

    /// Retrieves a quantity from the storage, given its ID, value type and order. The stored quantity must
    /// have the same type and order (checked by assert). If no quantity with this ID is stored, throws
    /// std::exception.
    /// \return Array of references to LimitedArrays, containing quantity values and derivatives.
    template <QuantityKey TKey, typename TValue, OrderEnum TOrder>
    auto get() {
        // linear search in array of quantities
        for (Quantity& q : quantities) {
            if (q.getKey() == TKey) {
                // found the right quantity
                auto optHolder = q.template cast<TValue, TOrder>();
                ASSERT(optHolder); // type or order mismatch!
                return optHolder->getBuffers();
            }
        }
        throw std::exception();
    }

    Quantity& operator[](const int idx) { return quantities[idx]; }

    const Quantity& operator[](const int idx) const { return quantities[idx]; }

    int getQuantityCnt() const { return quantities.size(); }


    int getParticleCnt() {
        // assuming positions R are ALWAYS present
        return particleCnt;
    }

    void merge(Storage& other) {
        // must contain the same quantities
        ASSERT(this->getQuantityCnt() == other.getQuantityCnt());
        iteratePair<VisitorEnum::ALL_BUFFERS>(quantities, other.quantities, [](auto&& ar1, auto&& ar2) {
            ar1.pushAll(ar2);
        });
    }

    /// Clears all highest level derivatives of quantities
    void init() {
        iterate<VisitorEnum::FIRST_ORDER>(quantities, [](auto&& UNUSED(v), auto&& dv) {
            using TValue = typename std::decay_t<decltype(dv)>::Type;
            dv.fill(TValue(0._f));
        });
        iterate<VisitorEnum::SECOND_ORDER>(quantities, [](auto&& UNUSED(v), auto&& UNUSED(dv), auto&& d2v) {
            using TValue = typename std::decay_t<decltype(d2v)>::Type;
            d2v.fill(TValue(0._f));
        });
    }

    Storage clone(const Flags<VisitorEnum> flags) const {
        Storage cloned;
        for (const Quantity& q : quantities) {
            cloned.quantities.push(q.clone(flags));
        }
        return cloned;
    }

    void swap(Storage& other, const Flags<VisitorEnum> flags) {
        ASSERT(this->getQuantityCnt() == other.getQuantityCnt());
        for (int i = 0; i < this->getQuantityCnt(); ++i) {
            quantities[i].swap(other.quantities[i], flags);
        }
    }

    template <VisitorEnum Type, typename TFunctor>
    friend void iterate(Storage& storage, TFunctor&& functor) {
        iterate<Type>(storage.quantities, std::forward<TFunctor>(functor));
    }

    template <VisitorEnum Type, typename TFunctor>
    friend void iteratePair(Storage& storage1, Storage& storage2, TFunctor&& functor) {
        iteratePair<Type>(storage1.quantities, storage2.quantities, std::forward<TFunctor>(functor));
    }
};

template <int... TKeys>
Storage makeStorage() {
    Storage storage;
    storage.template insert<TKeys...>();
    return storage;
}


NAMESPACE_SPH_END
