#pragma once

#include "objects/containers/Array.h"
#include "objects/containers/Tuple.h"
#include "storage/Iterate.h"
#include "storage/QuantityMap.h"
#include "system/Logger.h"

NAMESPACE_SPH_BEGIN


/// Base object for storing scalar, vector and tensor quantities of SPH particles.
class Storage : public Noncopyable {
private:
    Array<Quantity> quantities;

    template <int TKey, typename TGetter>
    Array<QuantityType<TKey>>& getSingle(TGetter&& getter) {
        // linear search in array of quantities
        for (Quantity& q : quantities) {
            if (q.getKey() == TKey) {
                // found the right quantity
                auto optHolder = q.template cast<QuantityType<TKey>, QuantityMap<TKey>::temporalEnum>();
                ASSERT(optHolder); // we got the type and time enum from map, it has to be correct
                return getter(optHolder.get());
            }
        }
        throw std::exception();
        // not found, return nullptr arrayview
        // return Array<QuantityType<TKey>>();
    }

public:
    Storage() = default;

    Storage(Storage&& other)
        : quantities(std::move(other.quantities)) {}

    Storage& operator=(Storage&& other) {
        quantities = std::move(other.quantities);
        return *this;
    }

    /// Inserts a list of quantities into the storage. Quantities are given by their index to template map
    /// QuantityMap, from which we get type and temporal enum of the quantity.
    template <int TKey, int TSecond, int... TRest>
    void insert() {
        this->insert<QuantityType<TKey>, QuantityMap<TKey>::temporalEnum>(TKey);
        this->insert<TSecond, TRest...>(); // recursive call
    }

    /// Inserts a single quantity into the storage. The type and temporal enum are given by the index to
    /// template QuantityMap.
    template <int TKey, typename... TArgs>
    void insert(TArgs&&... args) {
        this->insert<QuantityType<TKey>, QuantityMap<TKey>::temporalEnum>(TKey, std::forward<TArgs>(args)...);
    }

    /// Inserts "manually" a quantity, given their type and temporal type. Note that the quantity cannot be
    /// extracted from storage unless its index is in QuantityMap; it can be reached using iterate method,
    /// though.
    template <typename TValue, TemporalEnum TEnum, typename... TArgs>
    void insert(const int key, TArgs&&... args) {
        Quantity q;
        q.template emplace<TValue, TEnum>(key, std::forward<TArgs>(args)...);
        quantities.push(std::move(q));
    }

    /// Returns a list of at least two quantities (given by their indices) from the storage. Returns them as
    /// tuple of array references.
    template <int TFirst, int TSecond, int... TRest>
    Tuple<ArrayView<QuantityType<TFirst>>,
          ArrayView<QuantityType<TSecond>>,
          ArrayView<QuantityType<TRest>>...>
    get() {
        auto getter = [](auto&& holder) -> auto& { return holder.getValue(); };
        return makeTuple(this->template getSingle<TFirst>(getter).getView(),
                         this->template getSingle<TSecond>(getter).getView(),
                         this->template getSingle<TRest>(getter).getView()...);
    }

    /// Returns a list of derivatives
    template <int TFirst, int TSecond, int... TRest>
    Tuple<ArrayView<QuantityType<TFirst>>,
          ArrayView<QuantityType<TSecond>>,
          ArrayView<QuantityType<TRest>>...>
    dt() {
        auto getter = [](auto&& holder) -> auto& { return holder.getDerivative(); };
        return makeTuple(this->template getSingle<TFirst>(getter).getView(),
                         this->template getSingle<TSecond>(getter).getView(),
                         this->template getSingle<TRest>(getter).getView()...);
    }

    /// Returns a list of 2nd derivatives
    template <int TFirst, int TSecond, int... TRest>
    Tuple<ArrayView<QuantityType<TFirst>>,
          ArrayView<QuantityType<TSecond>>,
          ArrayView<QuantityType<TRest>>...>
    d2t() {
        auto getter = [](auto&& holder) -> auto& { return holder.get2ndDerivative(); };
        return makeTuple(this->template getSingle<TFirst>(getter).getView(),
                         this->template getSingle<TSecond>(getter).getView(),
                         this->template getSingle<TRest>(getter).getView()...);
    }

    /// Returns a quantity given by its index from the storage.
    template <int TKey>
    Array<QuantityType<TKey>>& get() {
        return this->template getSingle<TKey>([](auto&& holder) -> auto& { return holder.getValue(); });
    }

    /// Returns a derivative given by its index from the storage.
    template <int TKey>
    Array<QuantityType<TKey>>& dt() {
        return this->template getSingle<TKey>([](auto&& holder) -> auto& { return holder.getDerivative(); });
    }

    /// Returns a 2nd derivative given by its index from the storage.
    template <int TKey>
    Array<QuantityType<TKey>>& d2t() {
        return this->template getSingle<TKey>([](auto&& holder) -> auto& {
            return holder.get2ndDerivative();
        });
    }

    /// Returns views to all buffers stored in a quantity.
    template <int TKey>
    Array<LimitedArray<QuantityType<TKey>>&> getAll() {
        for (Quantity& q : quantities) {
            if (q.getKey() == TKey) {
                auto optHolder = q.template cast<QuantityType<TKey>, QuantityMap<TKey>::temporalEnum>();
                ASSERT(optHolder);
                return optHolder->getBuffers();
            }
        }
        throw std::exception();
    }

    int size() const { return quantities.size(); }

    int getParticleCnt() {
        // assuming positions R are ALWAYS present
        return this->get<QuantityKey::R>().size();
    }

    void merge(Storage& other) {
        // must contain the same quantities
        ASSERT(this->size() == other.size());
        iteratePair<TemporalEnum::ALL>(quantities, other.quantities, [](auto&& ar1, auto&& ar2) {
            ar1.pushAll(ar2);
        });
    }

    /// Clears all highest level derivatives of quantities
    void init() {
        iterate<TemporalEnum::FIRST_ORDER>(quantities, [](auto&& UNUSED(v), auto&& dv) {
            using TValue = typename std::decay_t<decltype(dv)>::Type;
            dv.fill(TValue(0._f));
        });
        iterate<TemporalEnum::SECOND_ORDER>(quantities, [](auto&& UNUSED(v), auto&& UNUSED(dv), auto&& d2v) {
            using TValue = typename std::decay_t<decltype(d2v)>::Type;
            d2v.fill(TValue(0._f));
        });
    }

    Storage clone(const Flags<TemporalEnum> flags) const {
        Storage cloned;
        for (const Quantity& q : quantities) {
            cloned.quantities.push(q.clone(flags));
        }
        return cloned;
    }

    void swap(Storage& other, const Flags<TemporalEnum> flags) {
        ASSERT(this->size() == other.size());
        for (int i = 0; i < this->size(); ++i) {
            quantities[i].swap(other.quantities[i], flags);
        }
    }

    template <TemporalEnum Type, typename TFunctor>
    friend void iterate(Storage& storage, TFunctor&& functor) {
        iterate<Type>(storage.quantities, std::forward<TFunctor>(functor));
    }

    template <TemporalEnum Type, typename TFunctor>
    friend void iteratePair(Storage& storage1, Storage& storage2, TFunctor&& functor) {
        iteratePair<Type>(storage1.quantities, storage2.quantities, std::forward<TFunctor>(functor));
    }

    /// \todo saving velocities?
    template <int... TKeys>
    void save(Abstract::Logger* output, const Float UNUSED(time)) {
        auto tuple     = this->get<TKeys...>();
        auto first     = tuple.template get<0>();
        const int size = first.size();
        for (int i = 0; i < size; ++i) {
            forEach(tuple, [output, i](auto&& array) {
                std::stringstream ss;
                ss << std::setw(15) << array[i];
                output->write(ss.str());
            });
            output->write("\n");
        }
    }
};

template <int... TKeys>
Storage makeStorage() {
    Storage storage;
    storage.template insert<TKeys...>();
    return storage;
}


NAMESPACE_SPH_END
