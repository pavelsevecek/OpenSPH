#pragma once

/// \file Storage.h
/// \brief Container for storing particle quantities and materials.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/ForwardDecl.h"
#include "objects/Exceptions.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/SharedPtr.h"
#include "quantities/Quantity.h"
#include "quantities/QuantityIds.h"
#include "thread/Pool.h"
#include "thread/ThreadLocal.h"
#include <map>


NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Material;
}

struct StorageElement {
    QuantityId id;
    Quantity& quantity;
};

/// Helper class for iterating over quantities stored in \ref Storage.
class StorageIterator {
private:
    using Iterator = std::map<QuantityId, Quantity>::iterator;

    Iterator iter;

public:
    StorageIterator(const Iterator iterator)
        : iter(iterator) {}

    StorageIterator& operator++() {
        ++iter;
        return *this;
    }

    StorageElement operator*() {
        return { iter->first, iter->second };
    }

    bool operator!=(const StorageIterator& other) const {
        return iter != other.iter;
    }
};

/// Helper class, provides functions \ref begin and \ref end, returning iterators to the first and last
/// quantity in \ref Storage, respectively.
class StorageSequence {
private:
    Storage& storage;

public:
    StorageSequence(Storage& storage);

    /// Returns iterator pointing to the beginning of the quantity storage. Dereferencing the iterator yields
    /// \ref StorageElement, holding the \ref QuantityId and the reference to the \ref Quantity.
    StorageIterator begin();

    /// Returns iterator pointing to the one-past-the-end element of the quantity storage.
    StorageIterator end();

    /// Returns the number of quantities.
    Size size() const;
};

/// \brief Container storing all quantities used within the simulations.
///
/// Storage provides a convenient way to store quantities, iterate over specified subset of quantnties, modify
/// quantities etc. Every quantity is a \ref Quantity object and is identified by \ref QuantityId key. The
/// quantities are stored as key-value pairs; for every \ref QuantityId there can be at most one \ref Quantity
/// stored.
///
/// Storage can contain scalar, vector, tensor and integer quantities. Every quantity can also have associated
/// one or two derivatives. There is no constraint on quantity order or type for given \ref QuantityId, i.e.
/// as far as \ref Storage object is concerned, one can create a QuantityId::ENERGY tensor quantity with
/// second derivatives or integer quantity QuantityId::SOUND_SPEED. Different parts of the code require
/// certain types for some quantities, though. Particle positions, QuantityId::POSITIONS, are mostly assumed
/// to be vector quantities of second order. Inconsistency of types will cause an assert when encountered.
///
/// Storage can hold arbitrary number of materials, objects derived from \ref Abstract::Material. In theory,
/// every particle can have a different material (different equation of state, different rheology, ...).
/// The storage can also exist with no material; this is a valid state, useful for situations where no
/// material is necessary. A storage with material can be created using constructor
/// Storage(AutoPtr<Abstract::Material>&& material). All particles subsequently added into the storage
/// will have the material passed in the parameter of the constructor. Storage with multiple materials can
/// then be created by merging the storage with another object, using function \ref merge.
class Storage : public Noncopyable {
    friend class StorageSequence;

private:
    /// Stored quantities (array of arrays). All arrays must be the same size at all times.
    std::map<QuantityId, Quantity> quantities;

    /// Holds materials of particles. Each particle can (in theory) have a different material.
    Array<AutoPtr<Abstract::Material>> materials;

    /// Partitions between the materials. The size of the array matches the size of materials, or it is empty,
    /// in which case all particles belongs to the same material.
    /// Particles of the first material are 0..partitions[0], section material partitions[0]..partitions[1],
    /// etc.
    Array<Size> partitions;

    /// Thread pool for parallelization
    SharedPtr<ThreadPool> pool;

public:
    /// Creates a storage with no material. Any call of \ref getMaterial function will result in assert.
    Storage();

    /// Initialize a storage with a material.
    Storage(AutoPtr<Abstract::Material>&& material);

    ~Storage();

    Storage(Storage&& other);

    Storage& operator=(Storage&& other);

    /// Checks if the storage contains quantity with given key. Type or order of unit is not specified.
    bool has(const QuantityId key) const {
        return quantities.find(key) != quantities.end();
    }

    /// Checks if the storage contains quantity with given key, value type and order.
    template <typename TValue>
    bool has(const QuantityId key, const OrderEnum order) const {
        auto iter = quantities.find(key);
        if (iter == quantities.end()) {
            return false;
        }
        const Quantity& q = iter->second;
        return q.getOrderEnum() == order && q.getValueEnum() == GetValueEnum<TValue>::type;
    }

    /// Retrieves quantity with given key from the storage. Quantity must be already stored, checked by
    /// assert.
    Quantity& getQuantity(const QuantityId key) {
        auto iter = quantities.find(key);
        ASSERT(iter != quantities.end(), getQuantityName(key));
        return iter->second;
    }

    /// Retrieves quantity with given key from the storage, const version.
    const Quantity& getQuantity(const QuantityId key) const {
        auto iter = quantities.find(key);
        ASSERT(iter != quantities.end());
        return iter->second;
    }

    /// Retrieves quantity buffers from the storage, given its key and value type. The stored quantity must be
    /// of type TValue, checked by assert. Quantity must already exist in the storage, checked by assert. To
    /// check whether the quantity is stored, use has() method.
    /// \return Array of references to Arrays, containing quantity values and all derivatives.
    template <typename TValue>
    StaticArray<Array<TValue>&, 3> getAll(const QuantityId key) {
        Quantity& q = this->getQuantity(key);
        ASSERT(q.getValueEnum() == GetValueEnum<TValue>::type);
        return q.getAll<TValue>();
    }

    template <typename TValue>
    StaticArray<const Array<TValue>&, 3> getAll(const QuantityId key) const {
        const Quantity& q = this->getQuantity(key);
        ASSERT(q.getValueEnum() == GetValueEnum<TValue>::type);
        return q.getAll<TValue>();
    }

    /// Retrieves a quantity values from the storage, given its key and value type. The stored quantity must
    /// be of type TValue, checked by assert. Quantity must already exist in the storage, checked by assert.
    /// Note that values of quantity are returned as stored and need not have physical meaning; physical
    /// values of quantity might be modified by material (rheology, damage model, ...). To get physical values
    /// of quantity for use in equations, use \ref getPhysicalValue.
    /// \return Array reference containing stored quantity values.
    template <typename TValue>
    Array<TValue>& getValue(const QuantityId key) {
        Quantity& q = this->getQuantity(key);
        ASSERT(q.getValueEnum() == GetValueEnum<TValue>::type);
        return q.getValue<TValue>();
    }

    /// Retrieves a quantity values from the storage, given its key and value type, const version.
    /// \todo test
    template <typename TValue>
    const Array<TValue>& getValue(const QuantityId key) const {
        return const_cast<Storage*>(this)->getValue<TValue>(key);
    }

    /// Returns the physical values of given quantity.
    template <typename TValue>
    Array<TValue>& getPhysicalValue(const QuantityId key) {
        Quantity& q = this->getQuantity(key);
        ASSERT(q.getValueEnum() == GetValueEnum<TValue>::type);
        return q.getPhysicalValue<TValue>();
    }

    template <typename TValue>
    const Array<TValue>& getPhysicalValue(const QuantityId key) const {
        return const_cast<Storage*>(this)->getPhysicalValue<TValue>(key);
    }

    /// Returns all buffers, using physical values instead of stored values.
    template <typename TValue>
    StaticArray<Array<TValue>&, 3> getPhysicalAll(const QuantityId key) {
        Quantity& q = this->getQuantity(key);
        ASSERT(q.getValueEnum() == GetValueEnum<TValue>::type);
        return q.getPhysicalAll<TValue>();
    }

    template <typename TValue>
    StaticArray<Array<TValue>&, 2> modify(const QuantityId key) {
        Quantity& q = this->getQuantity(key);
        ASSERT(q.getValueEnum() == GetValueEnum<TValue>::type);
        return q.modify<TValue>();
    }

    /// Retrieves a quantity derivative from the storage, given its key and value type. The stored quantity
    /// must be of type TValue, checked by assert. Quantity must already exist in the storage and must be
    /// first or second order, checked by assert.
    /// \return Array reference containing quantity derivatives.
    template <typename TValue>
    Array<TValue>& getDt(const QuantityId key) {
        Quantity& q = this->getQuantity(key);
        ASSERT(q.getValueEnum() == GetValueEnum<TValue>::type);
        return q.getDt<TValue>();
    }

    /// Retrieves a quantity derivative from the storage, given its key and value type, const version.
    template <typename TValue>
    const Array<TValue>& getDt(const QuantityId key) const {
        return const_cast<Storage*>(this)->getDt<TValue>(key);
    }

    /// Retrieves a quantity second derivative from the storage, given its key and value type. The stored
    /// quantity must be of type TValue, checked by assert. Quantity must already exist in the storage and
    /// must be second order, checked by assert.
    /// \return Array reference containing quantity second derivatives.
    template <typename TValue>
    Array<TValue>& getD2t(const QuantityId key) {
        Quantity& q = this->getQuantity(key);
        ASSERT(q.getValueEnum() == GetValueEnum<TValue>::type);
        return q.getD2t<TValue>();
    }

    template <typename TValue>
    Array<TValue>& getHighestDerivative(const QuantityId key) {
        Quantity& q = this->getQuantity(key);
        ASSERT(q.getValueEnum() == GetValueEnum<TValue>::type);
        switch (q.getOrderEnum()) {
        case OrderEnum::ZERO:
            return q.getValue<TValue>();
        case OrderEnum::FIRST:
            return q.getDt<TValue>();
        case OrderEnum::SECOND:
            return q.getD2t<TValue>();
        default:
            NOT_IMPLEMENTED;
        }
    }

    /// Retrieves an array of quantities from the key. The type of all quantities must be the same and equal
    /// to TValue, checked by assert.
    template <typename TValue, typename... TArgs>
    auto getValues(const QuantityId first, const QuantityId second, const TArgs... others) {
        return tie(getValue<TValue>(first), getValue<TValue>(second), getValue<TValue>(others)...);
    }

    /// Retrieves an array of quantities from the key, const version.
    template <typename TValue, typename... TArgs>
    auto getValues(const QuantityId first, const QuantityId second, const TArgs... others) const {
        // better not const_cast here as we are deducing return type
        return tie(getValue<TValue>(first), getValue<TValue>(second), getValue<TValue>(others)...);
    }

    /// Creates a quantity in the storage, given its key, value type and order. Quantity is resized and filled
    /// with default value. This cannot be used to set number of particles, the size of the quantity is set to
    /// match current particle number.
    /// If a quantity with given key already exists in the storage, function checks that the quantity type is
    /// the same; if it isn't, InvalidSetup exception is thrown. If the required order of quantity is larger
    /// than the one currently stored, additional derivatives are created with no assert nor exception,
    /// otherwise the order is unchanged. Value of the quantity is unchanged, there is no check that the
    /// current value is the same as the default value given as parameter.
    /// \tparam TValue Type of the quantity. Can be scalar, vector, tensor or traceless tensor.
    /// \param key Unique key of the quantity.
    /// \param TOrder Order (number of derivatives) associated with the quantity.
    /// \param defaultValue Value to which quantity is initialized. If the quantity already exists in the
    ///                     storage, the value is unused.
    /// \returns Reference to the inserted quantity.
    template <typename TValue>
    Quantity& insert(const QuantityId key, const OrderEnum order, const TValue& defaultValue) {
        if (this->has(key)) {
            Quantity& q = this->getQuantity(key);
            if (q.getValueEnum() != GetValueEnum<TValue>::type) {
                throw InvalidSetup("Inserting quantity already stored with different type");
            }
            if (q.getOrderEnum() < order) {
                q.setOrder(order);
            }
        } else {
            const Size particleCnt = getParticleCnt();
            ASSERT(particleCnt);
            quantities[key] = Quantity(order, defaultValue, particleCnt);
        }
        return quantities[key];
    }

    /// Creates a quantity in the storage, given array of values. The size of the array must match the number
    /// of particles. Derivatives of the quantity are set to zero. Cannot be used if there already is a
    /// quantity with the same key, checked by assert. If this is the first quantity inserted into the
    /// storage, it sets the number of particles; all quantities added after that must have the same size.
    /// \returns Reference to the inserted quantity.
    template <typename TValue>
    Quantity& insert(const QuantityId key, const OrderEnum order, Array<TValue>&& values) {
        ASSERT(!this->has(key));
        Quantity q(order, std::move(values));
        UNUSED_IN_RELEASE(const Size size = q.size();)
        quantities[key] = std::move(q);
        ASSERT(quantities.empty() || size == getParticleCnt()); // size must match sizes of other quantities
        return quantities[key];
    }

    void setThreadPool(const SharedPtr<ThreadPool>& pool);

    SharedPtr<ThreadPool> getThreadPool() const {
        return pool;
    }

    /// \todo this really shouldn't be inside the storage, instead create some utility function for it.
    template <typename TFunctor>
    void parallelFor(const Size n1, const Size n2, TFunctor&& functor) {
        if (pool) {
            const Size granularity = min<Size>(1000, max<Size>((n2 - n1) / pool->getThreadCnt(), 1));
            Sph::parallelFor(*pool, n1, n2, granularity, std::forward<TFunctor>(functor));
        } else {
            functor(n1, n2);
        }
    }

    /// Returns an object containing a reference to given material. The object can also be used to iterate
    /// over indices of particles belonging to given material.
    /// \param matIdx Index of given material in storage. Materials are stored in unspecified order; to get
    ///               material of given particle, use \ref getMaterialOfParticle.
    MaterialView getMaterial(const Size matIdx) const;

    /// Returns material view for material of given particle.
    MaterialView getMaterialOfParticle(const Size particleIdx) const;

    /// Returns the bounding range of given quantity. Provides an easy access to the material range without
    /// construcing intermediate object of \ref MaterialView, otherwise this function is equivalent to:
    /// \code
    /// getMaterial(matIdx)->range(id)
    /// \endcode
    Range getRange(const QuantityId id, const Size matIdx) const;

    /// Returns the sequence of quantities.
    StorageSequence getQuantities();

    /// Return the number of materials in the storage. Material indices from 0 to (getMaterialCnt() - 1) are
    /// valid input for \ref getMaterialView function.
    Size getMaterialCnt() const;

    /// Returns the number of stored quantities.
    Size getQuantityCnt() const;

    /// Returns the number of particles. The number of particle is always the same for all quantities.
    Size getParticleCnt() const;

    /// Merges another storage into this object. The passed storage is moved in the process. All materials in
    /// the merged storage are conserved; particles will keep the materials they had before the merge.
    /// The function invalidates any reference or \ref ArrayView to quantity values or derivatives. For this
    /// reason, storages can only be merged when setting up initial conditions or inbetween timesteps, never
    /// while evaluating solver!
    void merge(Storage&& other);

    /// Sets all highest-level derivatives of quantities to zero. Other values are unchanged.
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

    /// Checks whether the storage is in valid state, that is whether all quantities have the same number of
    /// particles.
    bool isValid() const;

private:
    IndexSequence getMaterialRange(const Size matId) const;
};

NAMESPACE_SPH_END
