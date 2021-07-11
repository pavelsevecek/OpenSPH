#pragma once

/// \file Accumulated.h
/// \brief Buffer storing quantity values accumulated by summing over particle pairs
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/ForwardDecl.h"
#include "objects/containers/Array.h"
#include "objects/geometry/TracelessTensor.h"
#include "objects/wrappers/Variant.h"
#include "quantities/QuantityIds.h"

NAMESPACE_SPH_BEGIN

enum class OrderEnum;

enum class BufferSource {
    /// Only a single derivative accumulates to this buffer
    UNIQUE,

    /// Multiple derivatives may accumulate into the buffer
    SHARED,
};

/// \brief Storage for accumulating derivatives.
///
/// Each thread shall own its own Accumulated storage. Each accumulated buffer is associated with a quantity
/// using QuantityId.
class Accumulated {
private:
    template <typename... TArgs>
    using HolderVariant = Variant<Array<TArgs>...>;

    using Buffer = HolderVariant<Size, Float, Vector, TracelessTensor, SymmetricTensor>;

    struct Element {
        /// ID of accumulated quantity, used to stored the quantity into the storage
        QuantityId id;

        /// Order, specifying whether we are accumulating values or derivatives
        OrderEnum order;

        /// Accumulated data
        Buffer buffer;
    };
    Array<Element> buffers;

    struct QuantityRecord {
        QuantityId id;
        bool unique;
    };

    /// Debug array, holding IDs of all quantities to check for uniqueness.
    Array<QuantityRecord> records;

public:
    /// \brief Creates a new storage with given ID.
    ///
    /// Should be called once for each thread when the solver is initialized.
    /// \param id ID of the accumulated quantity
    /// \param order Order of the quantity. Only highest order can be accumulated, this parameter is used to
    ///              ensure the derivative is used consistently.
    /// \param unique Whether this buffer is being accumulated by a single derivative. It has no effect on
    ///               the simulation, but ensures a consistency of the run (that we don't accumulate two
    ///               different velocity gradients, for example).
    template <typename TValue>
    void insert(const QuantityId id, const OrderEnum order, const BufferSource source);

    /// \brief Initialize all storages.
    ///
    /// Storages are resized if needed and cleared out of all previously accumulated values.
    void initialize(const Size size);

    /// \brief Returns the buffer of given quantity and given order.
    ///
    /// \note Accumulated can store only one buffer per quantity, so the order is not neccesary to retrive the
    /// buffer, but it is required to check that we are indeed returning the required order of quantity. It
    /// also makes the code more readable.
    template <typename TValue>
    Array<TValue>& getBuffer(const QuantityId id, const OrderEnum order);

    /// \brief Sums values of a list of storages.
    ///
    /// Storages must have the same number of buffers and the matching buffers must have the same type and
    /// same size.
    void sum(ArrayView<Accumulated*> others);

    /// \brief Sums values, concurently over different quantities
    void sum(IScheduler& scheduler, ArrayView<Accumulated*> others);

    /// \brief Stores accumulated values to corresponding quantities.
    ///
    /// The accumulated quantity must already exist in the storage and its order must be at least the order of
    /// the accumulated buffer. The accumulated buffer is cleared (filled with zeroes) after storing the
    /// values into the storage.
    void store(Storage& storage);

    Size getBufferCnt() const;

private:
    template <typename Type>
    Array<Iterator<Type>> getBufferIterators(const QuantityId id, ArrayView<Accumulated*> others);

    template <typename Type>
    void sumBuffer(Array<Type>& buffer1, const QuantityId id, ArrayView<Accumulated*> others);

    template <typename Type>
    void sumBuffer(IScheduler& scheduler,
        Array<Type>& buffer1,
        const QuantityId id,
        ArrayView<Accumulated*> others);

    bool hasBuffer(const QuantityId id, const OrderEnum order) const;
};


NAMESPACE_SPH_END
