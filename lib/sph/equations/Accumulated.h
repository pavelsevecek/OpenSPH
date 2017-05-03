#pragma once

#include "common/Assert.h"
#include "geometry/TracelessTensor.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/Variant.h"
#include "quantities/QuantityIds.h"
#include "quantities/Storage.h"
#include "thread/ThreadLocal.h"

NAMESPACE_SPH_BEGIN

/// Storage for accumulating derivatives. Each thread shall own its own Accumulated storage.
/// Each accumulated buffer is associated with a quantity using QuantityId, buffer is then stored as the
/// highest derivative.
class Accumulated {
private:
    template <typename... TArgs>
    using HolderVariant = Variant<Array<TArgs>...>;

    using Buffer = HolderVariant<Size, Float, Vector, TracelessTensor, Tensor>;

    struct Element {
        QuantityId id;
        Buffer buffer;
    };
    Array<Element> buffers;

public:
    Accumulated() = default;

    /// Creates a new storage with given ID. Should be called once for each thread when the solver is
    /// initialized.
    template <typename TValue>
    void insert(const QuantityId id) {
        for (Element& e : buffers) {
            if (e.id == id) {
                // already used
                return;
            }
        }
        buffers.push(Element{ id, Array<TValue>() });
    }

    /// Initialize all storages, resizing them if needed and clearing out all previously accumulated values.
    void initialize(const Size size) {
        for (Element& e : buffers) {
            forValue(e.buffer, [size](auto& values) {
                values.resize(size);
                using T = typename std::decay_t<decltype(values)>::Type;
                values.fill(T(0._f));
            });
        }
    }

    /// Misnomer, this value != quantity value, it's actually higest derivative
    template <typename TValue>
    Array<TValue>& getValue(const QuantityId id) {
        for (Element& e : buffers) {
            if (e.id == id) {
                Array<TValue>& values = e.buffer;
                ASSERT(!values.empty());
                return values;
            }
        }
        STOP;
    }

    /// Sums values of a list of storages. Storages must have the same number of buffers and the matching
    /// buffers must have the same type and same size.
    void sum(ArrayView<Accumulated*> others) {
        for (Element& e : buffers) {
            forValue(e.buffer, [this, &e, &others](auto& buffer) INL { //
                this->sumBuffer(buffer, e.id, others);
            });
        }
    }

    /// Sums values, concurently over different quantities
    void sum(ThreadPool& pool, ArrayView<Accumulated*> others) {
        for (Element& e : buffers) {
            forValue(e.buffer, [this, &pool, &e, &others](auto& buffer) INL { //
                this->sumBuffer(pool, buffer, e.id, others);
            });
        }
    }

    /// Stores accumulated values to corresponding quantities. If there is no quantity with corresponding key
    /// in the storage, it is created with zero order.
    /// \todo merge with sum somehow
    void store(Storage& storage) {
        for (Element& e : buffers) {
            forValue(e.buffer, [&e, &storage](auto& buffer) {
                using T = std::decay_t<decltype(buffer)>;
                // storage must already have the quantity, we cannot add quantities during the run because of
                // timestepping
                ASSERT(storage.has(e.id), getQuantityName(e.id));
                storage.getHighestDerivative<typename T::Type>(e.id) = std::move(buffer);
            });
        }
    }

    Size getBufferCnt() const {
        return buffers.size();
    }

private:
    template <typename Type>
    Array<Iterator<const Type>> getBufferIterators(const QuantityId id, ArrayView<Accumulated*> others) {
        Array<Iterator<const Type>> iterators;
        for (Accumulated* other : others) {
            auto iter = std::find_if(other->buffers.begin(), other->buffers.end(), [id](const Element& e) { //
                return e.id == id;
            });
            ASSERT(iter != other->buffers.end());
            const Array<Type>& buffer2 = iter->buffer;
            iterators.push(buffer2.begin());
        }
        return iterators;
    }

    template <typename Type>
    void sumBuffer(Array<Type>& buffer1, const QuantityId id, ArrayView<Accumulated*> others) {
        Array<Iterator<const Type>> iterators = this->getBufferIterators<Type>(id, others);
        for (Size i = 0; i < buffer1.size(); ++i) {
            Type sum = Type(0._f);
            for (Iterator<const Type>& iter : iterators) {
                sum += *iter;
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
        Array<Iterator<const Type>> iterators = this->getBufferIterators<Type>(id, others);
        parallelFor(pool, 0, buffer1.size(), 100, [&iterators, &buffer1](const Size i) {
            Type sum = Type(0._f);
            for (Iterator<const Type> iter : iterators) {
                sum += *(iter + i);
            }
            buffer1[i] += sum;
        });
    }
};


NAMESPACE_SPH_END
