#pragma once

/// Simplified implementation of std::unique_ptr, using only default deleter.
#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

template<typename T>
class UniquePtr {
private:
    T* ptr = nullptr;

public:
    UniquePtr() = default;
};

NAMESPACE_SPH_END
