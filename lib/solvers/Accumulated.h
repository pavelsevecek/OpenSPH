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

    /// Sums values of two storages. Storages must have the same number of buffers and the matching buffers
    /// must have the same type and same size.
    /// \todo optimize, sum all at once instead of by pairs
    void sum(const Accumulated& other) {
        ASSERT(buffers.size() == other.buffers.size());
        for (Element& e : buffers) {
            sumBuffer(e.buffer, e.id, other);
        }
    }

    /// Sums values, concurently over different quantities
    void sum(ThreadPool& pool, const Accumulated& other) {
        ASSERT(buffers.size() == other.buffers.size());
        parallelFor(pool, 0, buffers.size(), [this, &other](Size i) INL {
            ASSERT(i < buffers.size());
            sumBuffer(buffers[i].buffer, buffers[i].id, other);
        });
    }

    /// Stores accumulated values to corresponding quantities. If there is no quantity with corresponding key
    /// in the storage, it is created with zero order.
    /// \todo merge with sum somehow
    void store(Storage& storage) {
        for (Element& e : buffers) {
            forValue(e.buffer, [&e, &storage](auto& buffer) {
                using T = std::decay_t<decltype(buffer)>;
                if (!storage.has(e.id)) {
                    storage.insert<typename T::Type>(e.id, OrderEnum::ZERO, std::move(buffer));
                } else {
                    storage.getHighestDerivative<typename T::Type>(e.id) = std::move(buffer);
                }
            });
        }
    }

    Size getBufferCnt() const {
        return buffers.size();
    }

private:
    void sumBuffer(Buffer& b, const QuantityId id, const Accumulated& other) {
        forValue(b, [id, &other](auto& buffer1) {
            using T = std::decay_t<decltype(buffer1)>;
            auto iter = std::find_if(
                other.buffers.begin(), other.buffers.end(), [id](const Element& e) { return e.id == id; });
            ASSERT(iter != other.buffers.end());
            const T& buffer2 = iter->buffer;
            ASSERT(buffer1.size() == buffer2.size());
            for (Size j = 0; j < buffer1.size(); ++j) {
                buffer1[j] += buffer2[j];
            }
        });
    }
};

NAMESPACE_SPH_END
