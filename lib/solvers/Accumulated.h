#pragma once

#include "geometry/TracelessTensor.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/Variant.h"
#include "quantities/QuantityIds.h"
#include "quantities/Storage.h"
#include <map>

NAMESPACE_SPH_BEGIN

/// Storage for accumulating derivatives. Each thread shall own its own Accumulated storage.
/// Each accumulated buffer is associated with a quantity using QuantityIds, buffer is then stored as the
/// highest derivative.
class Accumulated {
private:
    template <typename... TArgs>
    using HolderVariant = Variant<Array<TArgs>...>;

    using Buffer = HolderVariant<Size, Float, Vector, TracelessTensor, Tensor>;

    std::map<QuantityIds, Buffer> buffers;

public:
    Accumulated() = default;

    /// Creates a new storage with given ID. Should be called once for each thread when the solver is
    /// initialized.
    template <typename TValue>
    void insert(const QuantityIds id, const Size size) {
        buffers.insert({ id, Array<TValue>(size) });
    }

    /// Initialize all storages, resizing them if needed and clearing out all previously accumulated values.
    void initialize(const Size size) {
        for (auto b : buffers) {
            forValue(b.second, [size](auto& values) {
                values.resize(size);
                using T = typename std::decay_t<decltype(values)>::Type;
                values.fill(T(0._f));
            });
        }
    }

    /// Returns reference to holded data. All derivatives should save this view as member variable and save
    /// the computed values using arrayviews of these.
    template <typename TValue>
    Array<TValue>& getValue(const QuantityIds id) {
        ASSERT(buffers.find(id) != buffers.end());
        return buffers[id];
    }

    /// Sums values of two storages. Storages must have the same number of buffers and the matching buffers
    /// must have the same type and same size.
    /// \todo optimize, sum all at once instead of by pairs
    void sum(const Accumulated& other) {
        ASSERT(buffers.size() == other.buffers.size());
        for (auto i : buffers) {
            forValue(i.second, [ id = i.first, &other ](auto& buffer1) {
                using T = std::decay_t<decltype(buffer1)>;
                auto iter = other.buffers.find(id);
                ASSERT(iter != other.buffers.end() && iter->second.has<T>());
                const T& buffer2 = iter->second;
                ASSERT(buffer1.size() == buffer2.size());
                for (Size j = 0; j < buffer1.size(); ++j) {
                    buffer1[j] += buffer2[j];
                }
            });
        }
    }

    void store(Storage& storage) {
        for (auto i : buffers) {
            forValue(i.second, [ id = i.first, &storage ](auto& buffer) {
                using T = std::decay_t<decltype(buffer)>;
                storage.getHighestDerivative<typename T::Type>(id) = std::move(buffer);
            });
        }
    }
};

NAMESPACE_SPH_END
