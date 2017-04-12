#pragma once

#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

// mapping function, need to be specialized by each type used as Key.
template <typename TKey>
Size arrayMapping(const TKey& key) {
    return Size(key);
}

template <typename TKey, typename TValue>
class ArrayMap {
private:
    Array<TValue> data;

public:
    ArrayMap() = default;

    INLINE TValue& operator[](const TKey& key) {
        const Size idx = arrayMapping(key);
        if (idx >= data.size()) {
            data.resize(idx + 1);
        }
        return data[idx];
    }

    INLINE const TValue& operator[](const TKey& key) const {
        const Size idx = arrayMapping(key);
        ASSERT(idx < data.size());
        return data[idx];
    }
};

NAMESPACE_SPH_END
