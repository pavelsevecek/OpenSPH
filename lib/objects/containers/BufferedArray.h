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

    operator Array<Type>&() { return buffers[idx]; }
    operator const Array<Type>&() const { return buffers[idx]; }

    Array<Type>& get() { return buffers[idx]; }

    const Array<Type>& get() const { return buffers[idx]; }

    Array<Type>* operator->() { return &buffers[idx]; }

    const Array<Type>* operator->() const { return &buffers[idx]; }
};

NAMESPACE_SPH_END
