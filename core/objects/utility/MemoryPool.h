#pragma once

#include "objects/containers/Array.h"

namespace Sph {

template <typename T, int ChunkSize>
class MemoryPool : public Noncopyable {
    typedef Array<T> Chunk;
    Array<Chunk> chunks;
    Size pos = 0;

public:
    MemoryPool() = default;

    ArrayView<T> alloc(const Size n) {
        ASSERT(n < ChunkSize);
        if (pos + n > size()) {
            pos = size() + n;
            chunks.emplaceBack(ChunkSize);
            return ArrayView<T>(&chunks.back()[0], n);
        } else {
            int posInChunk = pos - (size() - ChunkSize);
            T& data = chunks.back()[posInChunk];
            pos += n;
            return ArrayView<T>(&data, n);
        }
    }

    void clear() {
        chunks.clear();
        pos = 0;
    }

    Size size() const {
        return chunks.size() * ChunkSize;
    }
};


} // namespace Sph
