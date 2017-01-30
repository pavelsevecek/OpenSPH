#pragma once

#include "objects/containers/Array.h"
#include "quantities/Quantity.h"
#include "quantities/QuantityIds.h"
#include "objects/ForwardDecl.h"
#include <map>

NAMESPACE_SPH_BEGIN

/// Base object for storing scalar, vector and tensor quantities of SPH particles. Other parts of the code
/// simply point to stored arrays using ArrayView.
class Storage : public Noncopyable {
private:
    /// Stored quantities (array of arrays). All arrays must be the same size at all times.
    std::map<QuantityIds, Quantity> quantities;

    /// Holds materials of particles. Each particle can (in theory) have a different material.
    Array<Material> materials;

public:
    Storage();

    /// Initialize a storage with a material.
    Storage(const BodySettings& settings);

    ~Storage();

    Storage(Storage&& other);

    Storage& operator=(Storage&& other);

    /// Checks if the storage contains quantity with given key. Type or order of unit is not specified.
    bool has(const QuantityIds key) const { return quantities.find(key) != quantities.end(); }

    /// Checks if the storage contains quantity with given key, value type and order.
    template <typename TValue, OrderEnum TOrder>
    bool has(const QuantityIds key) const {
        auto iter = quantities.find(key);
        if (iter == quantities.end()) {
            return false;
        }
        const Quantity& q = iter->second;
        return q.getOrderEnum() == TOrder && q.getValueEnum() == GetValueEnum<TValue>::type;
    }

    /// Retrieves quantity with given key from the storage. Quantity must be already stored, checked by
    /// assert.
    Quantity& getQuantity(const QuantityIds key) {
        auto iter = quantities.find(key);
        ASSERT(iter != quantities.end());
        return iter->second;
    }

    /// Retrieves quantity buffers from the storage, given its key and value type. The stored quantity must be
    /// of type TValue, checked by assert. Quantity must already exist in the storage, checked by assert. To
    /// check whether the quantity is stored, use has() method.
    /// \return Array of references to Arrays, containing quantity values and all derivatives.
    template <typename TValue>
    auto getAll(const QuantityIds key) {
        Quantity& q = this->getQuantity(key);
        ASSERT(q.getValueEnum() == GetValueEnum<TValue>::type);
        return q.getBuffers<TValue>();
    }

    /// Retrieves a quantity values from the storage, given its key and value type. The stored quantity must
    /// be of type TValue, checked by assert. Quantity must already exist in the storage, checked by assert.
    /// \return Array reference containing quantity values.
    template <typename TValue>
    Array<TValue>& getValue(const QuantityIds key) {
        Quantity& q = this->getQuantity(key);
        ASSERT(q.getValueEnum() == GetValueEnum<TValue>::type);
        return q.getValue<TValue>();
    }

    /// Retrieves a quantity derivative from the storage, given its key and value type. The stored quantity
    /// must be of type TValue, checked by assert. Quantity must already exist in the storage and must be
    /// first or second order, checked by assert.
    /// \return Array reference containing quantity derivatives.
    template <typename TValue>
    Array<TValue>& getDt(const QuantityIds key) {
        Quantity& q = this->getQuantity(key);
        ASSERT(q.getValueEnum() == GetValueEnum<TValue>::type);
        return q.getDt<TValue>();
    }

    /// Retrieves an array of quantities from the key. The type of all quantities must be the same and equal
    /// to TValue, checked by assert.
    template <typename TValue, typename... TArgs>
    auto getValues(const QuantityIds first, const QuantityIds second, const TArgs... others) {
        return tie(getValue<TValue>(first), getValue<TValue>(second), getValue<TValue>(others)...);
    }

    /// Creates a quantity in the storage, given its key, value type and order. Quantity is resized and filled
    /// with default value. This cannot be used to set number of particles, the size of the quantity is set to
    /// match current particle number. Can be used to override existing quantity with the same key.
    /// \tparam TValue Type of the quantity. Can be scalar, vector, tensor or traceless tensor.
    /// \tparam TOrder Order (number of derivatives) associated with the quantity.
    /// \param key Unique key of the quantity.
    /// \param defaultValue Value to which quantity is initialized. If the quantity already exists in the
    ///                     storage, the value is unused.
    /// \param range Optional parameter specifying lower and upper bound of the quantity. Bound are enforced
    ///              by timestepping algorithm. By default, quantities are unbounded.
    template <typename TValue, OrderEnum TOrder>
    void emplace(const QuantityIds key,
        const TValue& defaultValue,
        const Range& range = Range::unbounded(),
        const Float minimal = 0._f) {
        const Size particleCnt = getParticleCnt();
        ASSERT(particleCnt);
        Quantity q;
        q.emplace<TValue, TOrder>(defaultValue, particleCnt, range, minimal);
        quantities[key] = std::move(q);
    }

    /// Creates a quantity in the storage, given array of values. The size of the array must match the number
    /// of particles. Derivatives of the quantity are set to zero. Can be used to override existing quantity
    /// with the same key. If this is the first quantity inserted into the storage, it sets
    /// the number of particles; all quantities added after that must have the same size.
    template <typename TValue, OrderEnum TOrder>
    void emplace(const QuantityIds key,
        Array<TValue>&& values,
        const Range range = Range::unbounded(),
        const Float minimal = 0._f) {
        Quantity q;
        q.emplace<TValue, TOrder>(std::move(values), range, minimal);
        UNUSED_IN_RELEASE(const Size size = q.size();)
        quantities[key] = std::move(q);
        if (quantities.size() == 1) {
            // set material ids; we have only one material, so set everything to zero
            if (!materials.empty()) {
                this->emplace<Size, OrderEnum::ZERO_ORDER>(QuantityIds::MATERIAL_IDX, 0);
            }
        } else {
            ASSERT(size == getParticleCnt()); // size must match sizes of other quantities
        }
    }

    /* struct WithFunctorTag {};

     /// Creates a quantity in the storage, given a functor. This functor is called for every particle, takes
     /// position of the particle (Vector) and index of the particle (int) as argument, and returns quantity
     /// value for given particle. This can only be used if particle positions (QuantityIds::R) already exists
     /// in the storage. Derivatives of the quantity are set to zero. Can be used to override existing
     quantity
     /// with the same key.
     template <typename TValue, OrderEnum TOrder, typename TFunctor>
     void emplace(const QuantityIds key,
         WithFunctorTag,
         TFunctor&& functor,
         const Range range = Range::unbounded()) {
         Array<Vector>& r = this->getValue<Vector>(QuantityIds::POSITIONS);
         Array<TValue> values(r.size());
         for (Size i = 0; i < r.size(); ++i) {
             values[i] = functor(r[i], i);
         }
         this->emplace<TValue, TOrder>(key, std::move(values), range);
     }*/

    ArrayView<Material> getMaterials() { return materials; }

    /// Returns the number of stored quantities.
    Size getQuantityCnt() const;

    /// Returns the number of particles. The number of particle is always the same for all quantities.
    Size getParticleCnt() const;

    /// Returns iterator at the beginning of quantity map. Dereferencing the iterator yields
    /// std::pair<QuantityIds, Quantity>.
    /// \todo remove
    auto begin() { return quantities.begin(); }

    /// Returns iterator at the past-to-end of quantity map. Dereferencing the iterator yields
    /// std::pair<QuantityIds, Quantity>.
    auto end() { return quantities.end(); }

    void merge(Storage&& other);

    /// Clears all highest level derivatives of quantities
    void init();

    /// Removes all particles with all quantities (including materials) from the storage. The storage is left
    /// is a state as if it was default-constructed.
    void removeAll();

    /// Clones specified buffers of the storage. Cloned (sub)set of buffers is given by flags. Cloned storage
    /// will have the same number of quantities and the order and types of quantities will match; if some
    /// buffer is excluded from cloning, it is simply left empty.
    Storage clone(const Flags<VisitorEnum> flags) const;

    /// Changes number of particles for all quantities stored in the storage. Storage must already contain at
    /// least one quantity, checked by assert.
    void resize(const Size newParticleCnt);

    /// Swap quantities or given subset of quantities between two storages.
    void swap(Storage& other, const Flags<VisitorEnum> flags);
};

NAMESPACE_SPH_END