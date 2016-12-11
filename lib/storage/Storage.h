#pragma once

#include "objects/containers/Array.h"
#include "objects/containers/Tuple.h"
#include "objects/wrappers/NonOwningPtr.h"
#include "physics/Eos.h"
#include "storage/Iterate.h"
#include "storage/QuantityKey.h"
#include "system/Factory.h"
#include "system/Logger.h"
#include "system/Settings.h"
/// \todo do not include eos, move material to cpp

NAMESPACE_SPH_BEGIN


/// Runtime settings and functions for given material. Should not hold any constant values, these can be
/// stored in Settings<BodySettingsIds>.
struct Material : public Noncopyable {
    std::unique_ptr<Abstract::Eos> eos; /// equation of state for given material

    Material() = default;

    Material(std::unique_ptr<Abstract::Eos>&& eos)
        : eos(std::move(eos)) {}

    Material(Material&& other)
        : eos(std::move(other.eos)) {}

    Material& operator=(Material&& other) {
        eos = std::move(other.eos);
        return *this;
    }
};


/// Base object for storing scalar, vector and tensor quantities of SPH particles. Other parts of the code
/// simply point to stored arrays using ArrayView.
class Storage : public Noncopyable {
private:
    /// Stored quantities (array of arrays). All arrays must be the same size at all times.
    std::map<QuantityKey, Quantity> quantities;

    /// Holds materials of particles. Each particle can (in theory) have a different material. Use method
    /// getMaterial to obtain material for given particle.
    Array<Material> materials;

    /// Number of particles (=size of each array).
    int particleCnt = 0;

public:
    Storage() = default;

    /// Create storage from body settings. This determines material of all particles in the storage. Particles
    /// keep the material when two storages are merged.
    Storage(const Settings<BodySettingsIds>& settings) {
        Material mat;
        mat.eos = Factory::getEos(settings);
        materials.push(std::move(mat));
    }

    Storage(Storage&& other)
        : quantities(std::move(other.quantities))
        , materials(std::move(materials))
        , particleCnt(other.particleCnt) {}

    Storage& operator=(Storage&& other) {
        quantities  = std::move(other.quantities);
        materials   = std::move(other.materials);
        particleCnt = other.particleCnt;
        return *this;
    }

    /// Checks if the storage contains quantity with given key. Type or order of unit is not specified.
    bool has(const QuantityKey key) const { return quantities.find(key) != quantities.end(); }

    /// Checks if the storage contains quantity with given key, value type and order.
    template <typename TValue, OrderEnum TOrder>
    bool has(const QuantityKey key) const {
        auto iter = quantities.find(key);
        if (iter == quantities.end()) {
            return false;
        }
        const Quantity& q = iter->second;
        return q.getOrderEnum() == TOrder && q.getValueEnum() == GetValueEnum<TValue>::type;
    }

    /// Retrieves a quantity from the storage, given its key and value type. The stored quantity must be of
    /// type TValue, checked by assert. Quantity must already exist in the storage, checked by assert. To
    /// check whether the quantity is stored, use has() method.
    /// \return Array of references to LimitedArrays, containing quantity values and all derivatives.
    template <typename TValue>
    auto getAll(const QuantityKey key) {
        auto iter = quantities.find(key);
        ASSERT(iter != quantities.end());
        Quantity& q = iter->second;
        ASSERT(q.getValueEnum() == GetValueEnum<TValue>::type);
        return q.getBuffers<TValue>();
    }

    /// Retrieves a quantity values from the storage, given its key and value type. The stored quantity must
    /// be of type TValue, checked by assert. Quantity must already exist in the storage, checked by assert.
    /// \return LimitedArray reference containing quantity values.
    template <typename TValue>
    LimitedArray<TValue>& getValue(const QuantityKey key) {
        auto iter = quantities.find(key);
        ASSERT(iter != quantities.end());
        Quantity& q = iter->second;
        ASSERT(q.getValueEnum() == GetValueEnum<TValue>::type);
        return q.getValue<TValue>();
    }

    /// Retrieves an array of quantities from the key. The type of all quantities must be the same and equal
    /// to TValue, checked by assert.
    template <typename TValue, typename... TArgs>
    auto getValues(const QuantityKey first, const QuantityKey second, const TArgs... others) {
        return tieToArray(getValue<TValue>(first), getValue<TValue>(second), getValue<TValue>(others)...);
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
    void emplace(const QuantityKey key, const TValue& defaultValue, const Range& range = Range::unbounded()) {
        ASSERT(particleCnt);
        Quantity q;
        q.emplace<TValue, TOrder>(defaultValue, particleCnt, range);
        quantities[key] = std::move(q);
    }

    /// Creates a quantity in the storage, given array of values. The size of the array must match the number
    /// of particles. Derivatives of the quantity are set to zero. Can be used to override existing quantity
    /// with the same key. If this is the first quantity inserted into the storage, it sets
    /// the number of particles; all quantities added after that must have the same size.
    template <typename TValue, OrderEnum TOrder>
    void emplace(const QuantityKey key, Array<TValue>&& values, const Range range = Range::unbounded()) {
        Quantity q;
        q.emplace<TValue, TOrder>(std::move(values), range);
        if (quantities.size() == 0) {
            // this is the first quantity, set up number of particles
            ASSERT(particleCnt == 0);
            particleCnt = q.size();
            // set material ids; we have only one material, so set everything to zero
            if (!materials.empty()) {
                this->emplace<int, OrderEnum::ZERO_ORDER>(QuantityKey::MAT_ID, 0);
            }
        } else {
            ASSERT(particleCnt > 0 && q.size() == particleCnt); // size must match sizes of other quantities
        }
        quantities[key] = std::move(q);
    }

    /// Creates a quantity in the storage, given a functor. This functor is called for every particle, takes
    /// position of the particle (Vector) and index of the particle (int) as argument, and returns quantity
    /// value for given particle. This can only be used if particle positions (QuantityKey::R) already exists
    /// in the storage. Derivatives of the quantity are set to zero. Can be used to override existing quantity
    /// with the same key.
    template <typename TValue, OrderEnum TOrder, typename TFunctor>
    void emplaceWithFunctor(const QuantityKey key,
        TFunctor&& functor,
        const Range range = Range::unbounded()) {
        Array<Vector>& r = this->getValue<Vector>(QuantityKey::R);
        Array<TValue> values(r.size());
        for (int i = 0; i < r.size(); ++i) {
            values[i] = functor(r[i], i);
        }
        this->emplace<TValue, TOrder>(key, std::move(values), range);
    }

    /// Returns the number of stored quantities.
    int getQuantityCnt() const { return quantities.size(); }

    /// Returns the number of particles. The number of particle is always the same for all quantities.
    int getParticleCnt() { return particleCnt; }

    /// Returns the material of given particle.
    Material& getMaterial(const int particleIdx) {
        ASSERT(!materials.empty());
        /// \todo profile and possibly optimize (cache matIdxs array)
        Array<int>& matIdxs = this->getValue<int>(QuantityKey::MAT_ID);
        return materials[matIdxs[particleIdx]];
    }

    /// Assigns materials to particles. Particle positions (QuantityKey::R) must already be stored, checked by
    /// assert. This will override previously assigned materials. Selector is a functor taking particle
    /// position and its index and must return index into the array of materials.
    template <typename TSelector>
    void setMaterial(Array<Material>&& mats, TSelector&& selector) {
        ASSERT((this->has<Vector, OrderEnum::SECOND_ORDER>(QuantityKey::R)));
        this->materials     = std::move(mats);
        ArrayView<Vector> r = this->getValue<Vector>(QuantityKey::R);
        if (!this->has(QuantityKey::MAT_ID)) {
            this->emplace<int, OrderEnum::ZERO_ORDER>(QuantityKey::MAT_ID, 0);
        }
        Array<int>& matIdxs = this->getValue<int>(QuantityKey::MAT_ID);
        matIdxs.resize(r.size());
        for (int i = 0; i < r.size(); ++i) {
            matIdxs[i] = selector(r[i], i);
        }
    }

    /// Returns iterator at the beginning of quantity map. Dereferencing the iterator yields
    /// std::pair<QuantityKey, Quantity>.
    auto begin() { return quantities.begin(); }

    /// Returns iterator at the past-to-end of quantity map. Dereferencing the iterator yields
    /// std::pair<QuantityKey, Quantity>.
    auto end() { return quantities.end(); }

    void merge(Storage&& other) {
        // must contain the same quantities
        ASSERT(this->getQuantityCnt() == other.getQuantityCnt());
        // as material id is an index to array, we have to increase indices before the merge
        if (this->has(QuantityKey::MAT_ID)) {
            ASSERT(other.has(QuantityKey::MAT_ID));
            Array<int>& matIdxs = other.getValue<int>(QuantityKey::MAT_ID);
            for (int& id : matIdxs) {
                id += this->materials.size();
            }
            materials.pushAll(std::move(other.materials));
        }

        // merge all quantities
        iteratePair<VisitorEnum::ALL_BUFFERS>(
            quantities, other.quantities, [](auto&& ar1, auto&& ar2) { ar1.pushAll(std::move(ar2)); });
        particleCnt += other.particleCnt;
    }

    /// Clears all highest level derivatives of quantities
    void init() {
        iterate<VisitorEnum::HIGHEST_DERIVATIVES>(quantities, [](auto&& dv) {
            using TValue = typename std::decay_t<decltype(dv)>::Type;
            dv.fill(TValue(0._f));
        });
    }

    /// Clones specified buffers of the storage. Cloned (sub)set of buffers is given by flags. Cloned storage
    /// will have the same number of quantities and the order and types of quantities will match; if some
    /// buffer is excluded from cloning, it is simply left empty.
    Storage clone(const Flags<VisitorEnum> flags) const {
        Storage cloned;
        for (const auto& q : quantities) {
            cloned.quantities[q.first] = q.second.clone(flags);
        }
        return cloned;
    }

    /// Changes number of particles for all quantities stored in the storage. If there is currently no
    /// quantity stored, this sets up number of particles of any quantity created using its default value
    /// (see method Storage::emplace(const TValue&, const Range&) ).
    /// \todo signal all accessors
    template <VisitorEnum Type>
    void resize(const int newParticleCnt) {
        iterate<Type>(quantities, [newParticleCnt](auto&& buffer) { buffer.resize(newParticleCnt); });
        particleCnt = newParticleCnt;
    }

    void swap(Storage& other, const Flags<VisitorEnum> flags) {
        ASSERT(this->getQuantityCnt() == other.getQuantityCnt());
        for (auto i1 = quantities.begin(), i2 = other.quantities.begin(); i1 != quantities.end();
             ++i1, ++i2) {
            i1->second.swap(i2->second, flags);
        }
    }

    template <VisitorEnum Type, typename TFunctor>
    friend void iterate(Storage& storage, TFunctor&& functor) {
        iterate<Type>(storage.quantities, std::forward<TFunctor>(functor));
    }

    template <typename TFunctor>
    friend void iterateWithPositions(Storage& storage, TFunctor&& functor) {
        iterateWithPositions(storage.quantities, std::forward<TFunctor>(functor));
    }

    template <VisitorEnum Type, typename TFunctor>
    friend void iteratePair(Storage& storage1, Storage& storage2, TFunctor&& functor) {
        iteratePair<Type>(storage1.quantities, storage2.quantities, std::forward<TFunctor>(functor));
    }
};

NAMESPACE_SPH_END
