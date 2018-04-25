#pragma once

/// \file Accumulated.h
/// \brief Buffer storing quantity values accumulated by summing over particle pairs
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "common/Assert.h"
#include "objects/containers/Array.h"
#include "objects/geometry/TracelessTensor.h"
#include "objects/wrappers/Variant.h"
#include "quantities/QuantityIds.h"
#include "quantities/Storage.h"
#include "system/Settings.h"
#include "thread/ThreadLocal.h"

NAMESPACE_SPH_BEGIN

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
    void insert(const QuantityId id, const OrderEnum order, const BufferSource source) {
        // add the buffer if not already present
        if (!this->hasBuffer(id, order)) {
            buffers.push(Element{ id, order, Array<TValue>() });
        }

        // check if we didn't call this more then once for 'unique' buffers
        auto recordIter = std::find_if(records.begin(), records.end(), [id](QuantityRecord& r) { //
            return r.id == id;
        });
        if (recordIter == records.end()) {
            records.push(QuantityRecord{ id, source == BufferSource::UNIQUE });
        } else {
            ASSERT(recordIter->id == id);
            if (source == BufferSource::UNIQUE || recordIter->unique) {
                // either the previous record was unique and we are adding another one, or the previous one
                // was shared and now we are adding unique
                ASSERT(false, "Another derivatives accumulates to quantity marked as unique");
            }
        }
    }

    /// \brief Initialize all storages.
    ///
    /// Storages are resized if needed and cleared out of all previously accumulated values.
    void initialize(const Size size) {
        for (Element& e : buffers) {
            forValue(e.buffer, [size](auto& values) {
                using T = typename std::decay_t<decltype(values)>::Type;
                if (values.size() != size) {
                    values.resize(size);
                    values.fill(T(0._f));
                } else {
                    // check that the array is really cleared
                    ASSERT(std::count(values.begin(), values.end(), T(0._f)) == values.size());
                }
            });
        }
    }

    /// \brief Returns the buffer of given quantity and given order.
    ///
    /// \note Accumulated can store only one buffer per quantity, so the order is not neccesary to retrive the
    /// buffer, but it is required to check that we are indeed returning the required order of quantity. It
    /// also makes the code more readable.
    template <typename TValue>
    Array<TValue>& getBuffer(const QuantityId id, const OrderEnum UNUSED_IN_RELEASE(order)) {
        for (Element& e : buffers) {
            if (e.id == id) {
                ASSERT(e.order == order);
                Array<TValue>& values = e.buffer;
                ASSERT(!values.empty());
                return values;
            }
        }
        STOP;
    }

    /// \brief Sums values of a list of storages.
    ///
    /// Storages must have the same number of buffers and the matching buffers must have the same type and
    /// same size.
    void sum(ArrayView<Accumulated*> others) {
        for (Element& e : buffers) {
            forValue(e.buffer, [this, &e, &others](auto& buffer) INL { //
                this->sumBuffer(buffer, e.id, others);
            });
        }
    }

    /// \brief Sums values, concurently over different quantities
    void sum(ThreadPool& pool, ArrayView<Accumulated*> others) {
        for (Element& e : buffers) {
            forValue(e.buffer, [this, &pool, &e, &others](auto& buffer) INL { //
                this->sumBuffer(pool, buffer, e.id, others);
            });
        }
    }

    /// \brief Stores accumulated values to corresponding quantities.
    ///
    /// The accumulated quantity must already exist in the storage and its order must be at least the order of
    /// the accumulated buffer. The accumulated buffer is cleared (filled with zeroes) after storing the
    /// values into the storage.
    void store(Storage& storage) {
        for (Element& e : buffers) {
            forValue(e.buffer, [&e, &storage](auto& buffer) {
                using T = typename std::decay_t<decltype(buffer)>::Type;
                // storage must already have the quantity, we cannot add quantities during the run because of
                // timestepping
                ASSERT(storage.has(e.id), getMetadata(e.id).quantityName);
                ASSERT(Size(storage.getQuantity(e.id).getOrderEnum()) >= Size(e.order));
                storage.getAll<T>(e.id)[Size(e.order)] = std::move(buffer);
                buffer.fill(T(0._f));
            });
        }
    }

    Size getBufferCnt() const {
        return buffers.size();
    }

private:
    template <typename Type>
    Array<Iterator<Type>> getBufferIterators(const QuantityId id, ArrayView<Accumulated*> others) {
        Array<Iterator<Type>> iterators;
        for (Accumulated* other : others) {
            auto iter = std::find_if(other->buffers.begin(), other->buffers.end(), [id](const Element& e) { //
                return e.id == id;
            });
            ASSERT(iter != other->buffers.end());
            Array<Type>& buffer2 = iter->buffer;
            iterators.push(buffer2.begin());
        }
        return iterators;
    }

    template <typename Type>
    void sumBuffer(Array<Type>& buffer1, const QuantityId id, ArrayView<Accumulated*> others) {
        Array<Iterator<Type>> iterators = this->getBufferIterators<Type>(id, others);
        const Type zero = Type(0._f);
        for (Size i = 0; i < buffer1.size(); ++i) {
            Type sum = zero;
            for (Iterator<Type>& iter : iterators) {
                if (*iter != zero) {
                    sum += *iter;
                    *iter = zero;
                }
                ++iter;
            }
            buffer1[i] += sum;
        }
    }

    template <typename Type>
    void sumBuffer(ThreadPool& pool,
        Array<Type>& buffer1,
        const QuantityId id,
        ArrayView<Accumulated*> others) {
        Array<Iterator<Type>> iterators = this->getBufferIterators<Type>(id, others);
        auto functor = [&iterators, &buffer1](const Size i) INL {
            Type sum(0._f);
            for (Iterator<Type> iter : iterators) {
                Type& x = *(iter + i);
                if (x != Type(0._f)) {
                    sum += x;
                    x = Type(0._f);
                }
            }
            buffer1[i] += sum;
        };
        parallelFor(pool, 0, buffer1.size(), functor);
    }

    bool hasBuffer(const QuantityId id, const OrderEnum UNUSED_IN_RELEASE(order)) const {
        for (const Element& e : buffers) {
            if (e.id == id) {
                // already used
                ASSERT(e.order == order, "Cannot accumulate both values and derivatives of quantity");
                return true;
            }
        }
        return false;
    }
};


NAMESPACE_SPH_END
