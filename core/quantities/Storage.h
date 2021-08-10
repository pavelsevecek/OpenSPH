#pragma once

/// \file Storage.h
/// \brief Container for storing particle quantities and materials.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/ForwardDecl.h"
#include "objects/Exceptions.h"
#include "objects/containers/Array.h"
#include "objects/containers/FlatMap.h"
#include "objects/wrappers/Flags.h"
#include "objects/wrappers/Function.h"
#include "objects/wrappers/Outcome.h"
#include "objects/wrappers/SharedPtr.h"
#include "quantities/QuantityIds.h"

NAMESPACE_SPH_BEGIN

class IMaterial;
class Quantity;
class Box;
class StorageSequence;
class ConstStorageSequence;
class Attractor;
enum class OrderEnum;
enum class VisitorEnum;

struct StorageElement {
    QuantityId id;
    Quantity& quantity;
};

struct ConstStorageElement {
    QuantityId id;
    const Quantity& quantity;
};

/// \brief Helper class for iterating over quantities stored in \ref Storage.
class StorageIterator {
private:
    using ActIterator = Iterator<FlatMap<QuantityId, Quantity>::Element>;

    ActIterator iter;

public:
    StorageIterator(const ActIterator iterator, Badge<StorageSequence>);

    StorageIterator& operator++();

    StorageElement operator*();

    bool operator==(const StorageIterator& other) const;

    bool operator!=(const StorageIterator& other) const;
};

/// \brief Helper class for iterating over quantities stored in \ref Storage, const version.
class ConstStorageIterator {
private:
    using ActIterator = Iterator<const FlatMap<QuantityId, Quantity>::Element>;

    ActIterator iter;

public:
    ConstStorageIterator(const ActIterator iterator, Badge<ConstStorageSequence>);

    ConstStorageIterator& operator++();

    ConstStorageElement operator*();

    bool operator==(const ConstStorageIterator& other) const;

    bool operator!=(const ConstStorageIterator& other) const;
};

/// \brief Helper class, provides functions \ref begin and \ref end, returning iterators to the first and last
/// quantity in \ref Storage, respectively.
class StorageSequence {
private:
    FlatMap<QuantityId, Quantity>& quantities;

public:
    StorageSequence(FlatMap<QuantityId, Quantity>& quantities, Badge<Storage>);

    /// \brief Returns iterator pointing to the beginning of the quantity storage.
    ///
    /// Dereferencing the iterator yields \ref StorageElement, holding the \ref QuantityId and the reference
    /// to the \ref Quantity.
    StorageIterator begin();

    /// \brief Returns iterator pointing to the one-past-the-end element of the quantity storage.
    StorageIterator end();

    /// \brief Returns the number of quantities.
    Size size() const;
};

/// \brief Helper class, provides functions \ref begin and \ref end, returning const iterators to the first
/// and last quantity in \ref Storage, respectively.
class ConstStorageSequence {
private:
    const FlatMap<QuantityId, Quantity>& quantities;

public:
    ConstStorageSequence(const FlatMap<QuantityId, Quantity>& quantities, Badge<Storage>);

    /// \brief Returns iterator pointing to the beginning of the quantity storage.
    ///
    /// Dereferencing the iterator yields \ref ConstStorageElement, holding the \ref QuantityId and the
    /// reference to the \ref Quantity.
    ConstStorageIterator begin();

    /// Returns iterator pointing to the one-past-the-end element of the quantity storage.
    ConstStorageIterator end();

    /// Returns the number of quantities.
    Size size() const;
};

/// \brief Base class for arbitrary data stored in the storage alongside particles
class IStorageUserData : public Polymorphic {};

/// \brief Exception thrown when accessing missing quantities, casting to different types, etc.
class InvalidStorageAccess : public Exception {
public:
    explicit InvalidStorageAccess(const QuantityId id);

    explicit InvalidStorageAccess(const std::string& message);
};

/// \brief Container storing all quantities used within the simulations.
///
/// Storage provides a convenient way to store quantities, iterate over specified subset of quantities, modify
/// quantities etc. Every quantity is a \ref Quantity object and is identified by \ref QuantityId key. The
/// quantities are stored as key-value pairs; for every \ref QuantityId there can be at most one \ref Quantity
/// stored.
///
/// Storage can contain scalar, vector, tensor and integer quantities. Every quantity can also have associated
/// one or two derivatives. There is no constraint on quantity order or type for given \ref QuantityId, i.e.
/// as far as \ref Storage object is concerned, one can create a QuantityId::ENERGY tensor quantity with
/// second derivatives or integer quantity QuantityId::SOUND_SPEED. Different parts of the code require
/// certain types for some quantities, though. Particle positions, QuantityId::POSITION, are mostly assumed
/// to be vector quantities of second order. Inconsistency of types will cause an assert when encountered.
///
/// Storage can hold arbitrary number of materials, objects derived from \ref IMaterial. In theory,
/// every particle can have a different material (different equation of state, different rheology, ...).
/// The storage can also exist with no material; this is a valid state, useful for situations where no
/// material is necessary. A storage with material can be created using constructor
/// Storage(AutoPtr<IMaterial>&& material). All particles subsequently added into the storage
/// will have the material passed in the parameter of the constructor. Storage with multiple materials can
/// then be created by merging the storage with another object, using function \ref merge.
///
/// Storage is not thread-safe. If used in multithreaded context, any calls of member functions must be
/// synchonized by the caller.
///
/// The following demostrates how to access the particle data:
/// \code
/// // Get particle masses from the storage
/// ArrayView<Float> m = storage.getValue<Float>(QuantityId::MASS);
/// std::cout << "Mass of 5th particle = " << m[5] << std::endl;
///
/// // Get particle velocities (=derivative of positions) and accelerations (=second derivative of positions)
/// ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
/// ArrayView<Vector> dv = storage.getD2t<Vector>(QuantityId::POSITION);
///
/// // To get values and all derivatives at once, we can use the getAll function. The function returns an
/// // array containing all buffers, which can be "decomposed" into individual variables using tie function
/// // (similarly to std::tie).
/// ArrayView<Vector> r;
/// tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION); // return value is "decomposed"
///
/// // Lastly, if we want to get multiple values (not derivatives) of the same type, we can use getValues. We
/// // can also utilize the function tie; make sure to list the variables in the same order as the IDs.
/// ArrayView<Float> rho, u;
/// tie(rho, u, m) = storage.getValues<Float>(QuantityId::DENSITY, QuantityId::ENERGY, QuantityId::MASS);
/// \endcode
///
/// When adding a new quantity into the storage, it is necessary to specify the type of the quantity and the
/// number of derivatives using \ref OrderEnum. Quantity can be either initialized by providing a single
/// default value (used for all particles), or an array of values; see functions \ref insert. To add arbitrary
/// quantity, use:
/// \code
/// // Fill the array of angular frequencies
/// Array<Vector> omega(storage.getParticleCnt());
/// omega.fill(Vector(0, 0, 5)); // initialize it to 5 rad/s parallel to z-axis.
/// // Add angular frequencies to the storage, evolved as first-order quantity
/// storage.insert<Vector>(QuantityId::ANGULAR_FREQUENCY, OrderEnum::FIRST, std::move(omega));
///
/// // Add moment of inertia (with no derivatives)
/// const SymmetricTensor I = Rigid::sphereInertia(3, 2); // moment of a sphere with mass 3kg and radius 2m
/// storage.insert<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA, OrderEnum::ZERO, I);
/// \endcode
///
/// In some cases, it is useful to read or modify all quantities in the storage, without the need to fetch
/// them manually using \ref getValue and related function. There are two different ways to iterate over
/// quantities stored in storage. You can use the function \ref getQuantities, which returns a range (pair of
/// iterators) and thus allows to visit quantities in a for-loop:
/// \code
/// for (StorageElement element : storage.getQuantities()) {
///     std::cout << "Quantity " << element.id << std::endl
///     std::cout << " - order " << int(element.quantity.getOrderEnum()) << std::endl;
///     std::cout << " - type  " << int(element.quantity.getValueEnum()) << std::endl;
/// }
/// \endcode
/// This approach can be utilized to access properties of the quantities (as in the example above), clone
/// quantities, etc. The downside is that we still need to know the value type to actually access the quantity
/// values. To overcome this problem and access the quantity values in generic (type-agnostic) way, consider
/// using the function \ref iterate:
/// \code
/// // Iterate over all first order quantities in the storage
/// iterate<VisitorEnum::FIRST_ORDER>(storage, [](QuantityId id, auto& values, auto& derivatives) {
///     // Values and derivatives are arrays with quantity values. The type of the values can be Float,
///     // Vector, SymmetricTensor, etc., depending on the actual type of the stored quantity. Therefore, we
///     // can only use generic code here (functions overload for all quantity types).
///     std::cout << "Quantity " << id << std::endl;
///     std::cout << "Particle 0: " << values[0] << ", derivative ", derivatives[0] << std::endl;
/// });
///
/// // Iterates over all arrays in the storage, meaning all quantity values and derivatives.
/// iterate<VisitorEnum::ALL_BUFFERS>(storage, [](auto& array) {
///     // Use decltype to determine the type of the array
///     using Type = std::decay_t<decltype(array)>::Type;
///
///     // Set all values and all derivatives to zero (zero vector, zero tensor)
///     array.fill(Type(0._f));
/// });
/// \endcode
/// Note that arguments of the provided functor differ for each \ref VisitorEnum.
class Storage : public Noncopyable {
private:
    /// \brief Stored quantities (array of arrays). All arrays must be the same size at all times.
    FlatMap<QuantityId, Quantity> quantities;

    /// \brief Holds information about a material and particles with this material.
    struct MatRange {
        /// Actual implementation of the material
        SharedPtr<IMaterial> material;

        /// First index of particle with this material
        Size from = 0;

        /// One-past-last index of particle with this material
        Size to = 0;

        MatRange() = default;

        MatRange(const SharedPtr<IMaterial>& material, const Size from, const Size to);
    };

    /// \brief Materials of particles in the storage.
    ///
    /// Particles of the same material are stored consecutively; first material always starts with index 0 and
    /// last material ends with index equal to the number of particles.
    Array<MatRange> mats;

    /// \brief Cached view of material IDs of particles.
    ///
    /// Used for fast access of material properties.
    ArrayView<Size> matIds;

    /// \brief Additional point masses that only interact with other particles gravitationally.
    Array<Attractor> attractors;

    /// \brief Dependent storages, modified when the number of particles of this storage is changed.
    ///
    /// Needed in order to keep the number of particles in dependent storages the same.
    Array<WeakPtr<Storage>> dependent;

    /// \brief Arbitrary data set by \ref setUserData.
    ///
    /// May be nullptr.
    SharedPtr<IStorageUserData> userData;

public:
    /// \brief Creates a storage with no material.
    ///
    /// Any call of \ref getMaterial function will result in an assert.
    Storage();

    /// \brief Initialize a storage with a material.
    ///
    /// All particles of the storage will have the same material. To create a heterogeneous storage, it is
    /// necessary to merge another storage object into this one, using \ref merge function.
    explicit Storage(const SharedPtr<IMaterial>& material);

    ~Storage();

    Storage(Storage&& other);

    Storage& operator=(Storage&& other);

    /// \brief Checks if the storage contains quantity with given key.
    ///
    /// Type or order of unit is not specified, any quantity with this key will match.
    bool has(const QuantityId key) const;

    /// \brief Checks if the storage contains quantity with given key, value type and order.
    template <typename TValue>
    bool has(const QuantityId key, const OrderEnum order) const;

    /// \brief Retrieves quantity with given key from the storage.
    ///
    /// Quantity must be already stored, function throws an exception otherwise.
    Quantity& getQuantity(const QuantityId key);

    /// \brief Retrieves quantity with given key from the storage, const version.
    const Quantity& getQuantity(const QuantityId key) const;

    /// \brief Retrieves quantity buffers from the storage, given its key and value type.
    ///
    /// The stored quantity must be of type TValue, checked by assert. Quantity must already exist in the
    /// storage, checked by assert. To check whether the quantity is stored, use has() method.
    /// \return Array of references to Arrays, containing quantity values and all derivatives.
    template <typename TValue>
    StaticArray<Array<TValue>&, 3> getAll(const QuantityId key);

    /// \brief Retrieves quantity buffers from the storage, given its key and value type, const version.
    template <typename TValue>
    StaticArray<const Array<TValue>&, 3> getAll(const QuantityId key) const;

    /// \brief Retrieves a quantity values from the storage, given its key and value type.
    ///
    /// The stored quantity must be of type TValue, checked by assert. Quantity must already exist in the
    /// storage, checked by assert.
    /// \return Array reference containing stored quantity values.
    template <typename TValue>
    Array<TValue>& getValue(const QuantityId key);

    /// \copydoc getValue
    template <typename TValue>
    const Array<TValue>& getValue(const QuantityId key) const;

    /// \brief Retrieves a quantity derivative from the storage, given its key and value type.
    ///
    /// The stored quantity must be of type TValue, checked by assert. Quantity must already exist in the
    /// storage and must be first or second order, checked by assert.
    /// \return Array reference containing quantity derivatives.
    template <typename TValue>
    Array<TValue>& getDt(const QuantityId key);

    /// \brief Retrieves a quantity derivative from the storage, given its key and value type, const version.
    template <typename TValue>
    const Array<TValue>& getDt(const QuantityId key) const;

    /// \brief Retrieves a quantity second derivative from the storage, given its key and value type.
    ///
    /// The stored quantity must be of type TValue, checked by assert. Quantity must already exist in the
    /// storage and must be second order, checked by assert.
    /// \return Array reference containing quantity second derivatives.
    template <typename TValue>
    Array<TValue>& getD2t(const QuantityId key);

    /// \copydoc getD2t
    template <typename TValue>
    const Array<TValue>& getD2t(const QuantityId key) const;

    /// \brief Retrieves an array of quantities from the key.
    ///
    /// The type of all quantities must be the same and equal to TValue, checked by assert.
    template <typename TValue, typename... TArgs>
    auto getValues(const QuantityId first, const QuantityId second, const TArgs... others) {
        return tie(getValue<TValue>(first), getValue<TValue>(second), getValue<TValue>(others)...);
    }

    /// \brief Retrieves an array of quantities from the key, const version.
    template <typename TValue, typename... TArgs>
    auto getValues(const QuantityId first, const QuantityId second, const TArgs... others) const {
        // better not const_cast here as we are deducing return type
        return tie(getValue<TValue>(first), getValue<TValue>(second), getValue<TValue>(others)...);
    }

    /// \brief Creates a quantity in the storage, given its key, value type and order.
    ///
    /// Quantity is resized and filled with default value. This cannot be used to set number of particles, the
    /// size of the quantity is set to match current particle number. If the quantity is already stored in the
    /// storage, function only checks that the type of the quantity matches, but otherwise keeps the
    /// previously stored values. If the required order of quantity is larger than the one currently stored,
    /// additional derivatives are created with no assert nor exception, otherwise the order is unchanged.
    /// \tparam TValue Type of the quantity. Can be scalar, vector, tensor or traceless tensor.
    /// \param key Unique key of the quantity.
    /// \param TOrder Order (number of derivatives) associated with the quantity.
    /// \param defaultValue Value to which quantity is initialized. If the quantity already exists in the
    ///                     storage, the value is unused.
    /// \returns Reference to the inserted quantity.
    template <typename TValue>
    Quantity& insert(const QuantityId key, const OrderEnum order, const TValue& defaultValue);

    /// \brief Creates a quantity in the storage, given array of values.
    ///
    /// The size of the array must match the number of particles. Derivatives of the quantity are set to zero.
    /// If this is the first quantity inserted into the storage, it sets the number of particles; all
    /// quantities added after that must have the same size. If a quantity with the same key already exists in
    /// the storage, its values are overriden. Derivatives are not changed. In this case, the function checks
    /// that the quantity type is the same; if it isn't, InvalidSetup exception is thrown.
    /// \returns Reference to the inserted quantity.
    template <typename TValue>
    Quantity& insert(const QuantityId key, const OrderEnum order, Array<TValue>&& values);

    /// \brief Adds a point-mass attractor to the storage.
    void addAttractor(const Attractor& a);

    /// \brief Registers a dependent storage.
    ///
    /// A dependent storage mirrors changes of particle counts. Every time new particles are added into the
    /// storage or when some particles are removed, the same action is performed on all (existing) dependent
    /// storages. However, no other action is handled this way, namely new quantities have to be added
    /// manually to all storages. Same goes for clearing the derivatives, changing materials, etc.
    ///
    /// Note that the storage holds weak references, the dependent storages must be held in SharedPtr
    /// somewhere to keep the link.
    void addDependent(const WeakPtr<Storage>& other);

    /// \brief Returns an object containing a reference to given material.
    ///
    /// The object can also be used to iterate over indices of particles belonging to given material.
    /// \param matIdx Index of given material in storage. Materials are stored in unspecified order; to get
    ///               material of given particle, use \ref getMaterialOfParticle.
    MaterialView getMaterial(const Size matIdx) const;

    /// \brief Returns material view for material of given particle.
    MaterialView getMaterialOfParticle(const Size particleIdx) const;

    /// \brief Modifies material with given index.
    ///
    /// The new material cannot be nullptr.
    void setMaterial(const Size matIdx, const SharedPtr<IMaterial>& material);

    /// \brief Returns the bounding range of given quantity.
    ///
    /// Provides an easy access to the material range without construcing intermediate object of \ref
    /// MaterialView, otherwise this function is equivalent to: \code getMaterial(matIdx)->range(id) \endcode
    Interval getRange(const QuantityId id, const Size matIdx) const;

    /// \brief Returns the given material parameter for all materials in the storage.
    ///
    /// To get the material parameter for given particle, use the index given by material ID.
    template <typename TValue>
    Array<TValue> getMaterialParams(const BodySettingsId param) const;

    /// \brief Checks if the particles in the storage are homogeneous with respect to given parameter.
    ///
    /// It is assumed that the parameter is scalar.
    bool isHomogeneous(const BodySettingsId param) const;

    /// \brief Returns the sequence of quantities.
    StorageSequence getQuantities();

    /// \brief Returns the sequence of quantities, const version.
    ConstStorageSequence getQuantities() const;

    /// \brief Returns the sequence of stored point-mass attractors.
    ArrayView<Attractor> getAttractors();

    /// \brief Returns the sequence of stored point-mass attractors, const version.
    ArrayView<const Attractor> getAttractors() const;

    /// \brief Executes a given functor recursively for all dependent storages.
    ///
    /// This storage is not visited, the functor is executed with child storages, grandchild storages, etc. If
    /// one of dependent storages expired (no shared pointer is currently holding it), it is removed from the
    /// list.
    void propagate(const Function<void(Storage& storage)>& functor);

    /// \brief Return the number of materials in the storage.
    ///
    /// Material indices from 0 to (getMaterialCnt() - 1) are valid input for \ref getMaterialView function.
    Size getMaterialCnt() const;

    /// \brief Returns the number of stored quantities.
    Size getQuantityCnt() const;

    /// \brief Returns the number of particles.
    ///
    /// The number of particle is always the same for all quantities.
    /// \warning This count does not include the number of attractors.
    Size getParticleCnt() const;

    /// \brief Returns the number of attractors.
    Size getAttractorCnt() const;

    /// \brief Checks if the storage is empty, i.e. without particles.
    bool empty() const;

    /// \brief Merges another storage into this object.
    ///
    /// The passed storage is moved in the process. All materials in the merged storage are conserved;
    /// particles will keep the materials they had before the merge. The merge is only allowed for storages
    /// that both have materials or neither have one. Merging a storage without a material into a storage with
    /// at least one material will result in assert. Similarly for merging a storage with materials into a
    /// storage without materials.
    ///
    /// The function invalidates any reference or \ref ArrayView to quantity values or derivatives. For this
    /// reason, storages can only be merged when setting up initial conditions or inbetween timesteps, never
    /// while evaluating solver!
    void merge(Storage&& other);

    /// \brief Sets all highest-level derivatives of quantities to zero.
    ///
    /// Other values are unchanged.
    void zeroHighestDerivatives();

    enum class IndicesFlag {
        /// Use if the given array is already sorted (optimization)
        INDICES_SORTED = 1 << 0,
    };

    /// \brief Duplicates some particles in the storage.
    ///
    /// New particles are added to an unspecified positions in the storage, copying all the quantities and
    /// materials from the source particles. Note that this is not intended to be used without further
    /// modifications of the newly created particles as we never want to create particle pairs; make sure to
    /// move the created particles to required positions and modify their quantities as needed. The function
    /// can be used to add new particles with materials already existing in the storage.
    /// \param idxs Indices of the particles to duplicate.
    /// \return Indices of the newly created particles (in the modified storage). Note that the original
    ///         indices passed into the storage are no longer valid after the function is called.
    Array<Size> duplicate(ArrayView<const Size> idxs, const Flags<IndicesFlag> flags = EMPTY_FLAGS);

    /// \brief Removes specified particles from the storage.
    ///
    /// If all particles of some material are removed by this, the material is also removed from the storage.
    /// Same particles are also removed from all dependent storages.
    /// \param idxs Indices of particles to remove. No need to sort the indices.
    void remove(ArrayView<const Size> idxs, const Flags<IndicesFlag> flags = EMPTY_FLAGS);

    /// \brief Removes all particles with all quantities (including materials) from the storage.
    ///
    /// The storage is left is a state as if it was default-constructed. Dependent storages are also cleared.
    void removeAll();

    /// \brief Clones specified buffers of the storage.
    ///
    /// Cloned (sub)set of buffers is given by flags. Cloned storage will have the same number of quantities
    /// and the order and types of quantities will match; if some buffer is excluded from cloning, it is
    /// simply left empty.
    /// Note that materials are NOT cloned, but rather shared with the parent storage. Modifying material
    /// parameters in cloned storage will also modify the parameters in the parent storage.
    Storage clone(const Flags<VisitorEnum> buffers) const;

    /// Options for the storage resize
    enum class ResizeFlag {
        /// Empty buffers will not be resized to new values.
        KEEP_EMPTY_UNCHANGED = 1 << 0,
    };

    /// \brief Changes number of particles for all quantities stored in the storage.
    ///
    /// If the new number of particles is larger than the current one, the quantities of the newly created
    /// particles are set to zero, regardless of the actual initial value. This is true for all quantity
    /// values and derivatives. Storage must already contain at least one quantity, checked by assert.
    /// All dependent storages are resized as well. Can be only used on storages with no material or storages
    /// with only a single material, checked by assert.
    /// \param newParticleCnt New number of particles.
    /// \param flags Options of the resizing, see ResizeFlag enum. By default, all quantities are resized.
    void resize(const Size newParticleCnt, const Flags<ResizeFlag> flags = EMPTY_FLAGS);

    /// \brief Swap quantities or given subset of quantities between two storages.
    ///
    /// Note that materials of the storages are NOT changed.
    void swap(Storage& other, const Flags<VisitorEnum> flags);

    enum class ValidFlag {
        /// Checks that the storage is complete, i.e. there are no empty buffers.
        COMPLETE = 1 << 0,
    };

    /// \brief Checks whether the storage is in valid state.
    ///
    /// The valid state means that all quantities have the same number of particles and materials are stored
    /// consecutively in the storage. This should be handled automatically, the function is mainly for
    /// debugging purposes.
    Outcome isValid(const Flags<ValidFlag> flags = ValidFlag::COMPLETE) const;

    /// \brief Stores new user data into the storage.
    ///
    /// Previous user data are overriden.
    void setUserData(SharedPtr<IStorageUserData> newData);

    /// \brief Returns the stored user data.
    ///
    /// If no data are stored, the function returns nullptr.
    SharedPtr<IStorageUserData> getUserData() const;

private:
    /// \brief Inserts all quantities contained in source storage that are not present in this storage.
    ///
    /// All added quantities are initialized to zero.
    void addMissingBuffers(const Storage& source);

    /// \brief Updates the cached matIds view.
    void update();
};

/// \brief Convenience function to get the bounding box of all particles.
///
/// This takes into account particle radii, using given kernel radius.
Box getBoundingBox(const Storage& storage, const Float radius = 2._f);

/// \brief Returns the center of mass of all particles.
///
/// Function can be called even if the storage does not store particle masses, in which case all particles are
/// assumed to have equal mass.
Vector getCenterOfMass(const Storage& storage);

/// \brief Adds or updates a quantity holding particle indices to the storage.
///
/// The indices are accessible through quantity QuantityId::PERSISTENT_INDEX. Initially, particle are numbered
/// from 0 to #particleCnt - 1, but the indices are persistent, meaning they remain unchanged when removing
/// particles from the storage. When a particle is removed from the storage, its index can be then re-used by
/// another particle added into the storage.
///
/// When merging two storages, indices remain unchanged in the storage where the particles are into, but they
/// are reset in the storage being merged, as they could be collisions of indices. Same goes to particles
/// added via \ref duplicate function.
void setPersistentIndices(Storage& storage);

NAMESPACE_SPH_END
