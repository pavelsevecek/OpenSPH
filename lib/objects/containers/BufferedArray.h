#pragma once

#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

template <typename Type>
class BufferedArray : public Noncopyable {
private:
    Array<Type> buffers[2];
    int idx = 0;


public:
    BufferedArray() = default;

    BufferedArray(BufferedArray&& other) {
        buffers[0] = std::move(other.buffers[0]);
        buffers[1] = std::move(other.buffers[1]);
        idx        = other.idx;
    }

    BufferedArray(Array<Type>&& other) { buffers[0] = std::move(other); }

    BufferedArray& operator=(BufferedArray&& other) {
        buffers[0] = std::move(other.buffers[0]);
        buffers[1] = std::move(other.buffers[1]);
        idx        = other.idx;
        return *this;
    }

    BufferedArray& operator=(Array<Type>&& other) {
        buffers[idx] = std::move(other);
        return *this;
    }

    void swap() { idx = (idx + 1) % 2; }

    /// Implicit conversion to top array
    operator Array<Type>&() { return buffers[idx]; }

    /// Implicit conversion to top array, const version
    operator const Array<Type>&() const { return buffers[idx]; }

    /// Returns a reference to top array
    Array<Type>& first() { return buffers[idx]; }

    /// Returns a reference to top array, const version
    const Array<Type>& first() const { return buffers[idx]; }

    /// Retursn a reference to buffered (bottom) array
    Array<Type>& second() { return buffers[(idx + 1) % 2]; }

    /// Returns a reference to buffered (bottom) array, const version
    const Array<Type>& second() const { return buffers[(idx + 1) % 2]; }

    Array<Type>* operator->() { return &buffers[idx]; }

    const Array<Type>* operator->() const { return &buffers[idx]; }
};

NAMESPACE_SPH_END
