#pragma once

#include "objects/containers/Array.h"
#include "objects/wrappers/Shadow.h"

NAMESPACE_SPH_BEGIN

/// Non-owning wrapper over float array, reinterpreting values as vectors to utilize SSE optimizations. Can be
/// only constructed if the size of the array is a multiple of 4 (checked by assert).
/// \todo specialize the array for vectors (would simply return the array itself), to provide generic usage?
/// \todo how to prevent invalidating parent object? (reallocating)
///       someday: don't expose array<vector> directly, instead put only selected functions (begin, end, size,
///       []) to interface

class VectorizedArray : public Noncopyable {
private:
    Shadow<Array<Vector>> storage;

public:
    VectorizedArray(Array<Float>& array) {
        ASSERT((array.size() % 4) == 0);
        // copy pointer to storage, without using Array<> constructor or destructor
        storage->data    = reinterpret_cast<Vector*>(array.data);
        storage->actSize = array.actSize / 4;
        storage->maxSize = storage->actSize;
    }

    Array<Vector>& get() { return storage.get(); }

    const Array<Vector>& get() const { return storage.get(); }

    Array<Vector>* operator->() { return storage.operator->(); }

    const Array<Vector>* operator->() const { return storage.operator->(); }
};

NAMESPACE_SPH_END
