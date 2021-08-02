#pragma once

/// \file BasicAllocators.h
/// \brief Allocators used by containers
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/Assert.h"
#include <mm_malloc.h>

NAMESPACE_SPH_BEGIN

struct MemoryBlock {
    void* ptr;
    std::size_t size;

    MemoryBlock() = default;
    MemoryBlock(void* ptr, const std::size_t size)
        : ptr(ptr)
        , size(size) {}

    INLINE static MemoryBlock EMPTY() {
        return { nullptr, 0 };
    }
};

template <typename T, typename TAllocator, typename... TArgs>
T* allocatorNew(TAllocator& allocator, TArgs&&... args) {
    MemoryBlock block = allocator.allocate(sizeof(T), alignof(T));
    if (block.ptr) {
        return new (block.ptr) T(std::forward<TArgs>(args)...);
    } else {
        return nullptr;
    }
}

template <typename T, typename TAllocator>
void allocatorDelete(TAllocator& allocator, T* ptr) {
    if (!ptr) {
        return;
    }

    ptr->~T();
    MemoryBlock block(ptr, sizeof(T));
    allocator.deallocate(block);
}

INLINE constexpr std::size_t roundToAlignment(const std::size_t value, const std::size_t align) noexcept {
    const std::size_t remainder = value % align;
    return value + ((remainder == 0) ? 0 : (align - remainder));
}

template <typename T>
INLINE T* roundToAlignment(T* value, const std::size_t align) noexcept {
    const std::size_t remainder = reinterpret_cast<std::size_t>(value) % align;
    return value + ((remainder == 0) ? 0 : (align - remainder));
}

INLINE bool isAligned(const std::size_t value, const std::size_t align) noexcept {
    return value % align == 0;
}

template <typename T>
INLINE bool isAligned(const T* value, const std::size_t align) noexcept {
    return reinterpret_cast<std::size_t>(value) % align == 0;
}

/// \brief Default allocator, simply wrapping _mm_malloc and _mm_free calls.
class Mallocator {
public:
    INLINE MemoryBlock allocate(const std::size_t size, const std::size_t align) noexcept {
        MemoryBlock block;
        block.ptr = _mm_malloc(size, align);
        if (block.ptr) {
            block.size = size;
        } else {
            block.size = 0;
        }
        return block;
    }

    INLINE void deallocate(MemoryBlock& block) noexcept {
        _mm_free(block.ptr);
        block.ptr = nullptr;
    }
};

/// \brief Allocator used pre-allocated fixed-size buffer on stack.
///
/// Function \ref allocate returns an empty block if the allocator does not have enough available memory.
template <std::size_t TSize, Size TAlign = 16>
class StackAllocator : public Immovable, Local {
private:
    alignas(TAlign) uint8_t data[TSize];

    /// Current position in the buffer
    uint8_t* pos = data;

public:
    INLINE MemoryBlock allocate(const std::size_t size, const std::size_t UNUSED(align)) noexcept {
        SPH_ASSERT(size > 0);

        const std::size_t actSize = roundToAlignment(size, TAlign);
        if (pos - data + actSize > TSize) {
            return MemoryBlock::EMPTY();
        }

        void* ptr = pos;
        pos += size;
        return { ptr, size };
    }

    INLINE void deallocate(MemoryBlock& block) noexcept {
        if (!block.ptr) {
            return;
        }
        if ((uint8_t*)block.ptr + block.size == pos) {
            pos = static_cast<uint8_t*>(block.ptr);
        }
        block = MemoryBlock::EMPTY();
    }

    INLINE bool owns(const MemoryBlock& block) const noexcept {
        return block.ptr >= data && block.ptr < data + TSize;
    }
};

/// \brief Allocator that attemps to allocate using given primary allocator, and if the allocation fails, it
/// allocates using a second fallback allocator.
///
/// Note that this is just a simple compositor and it doesn't guarantee that the fallback allocation succeeds.
/// To ensure the allocation is successful, consider nesting multiple fallback allocators together and provide
/// an allocator that cannot fail as the last one.
template <typename TPrimary, typename TFallback>
class FallbackAllocator : private TPrimary, private TFallback {
public:
    INLINE MemoryBlock allocate(const std::size_t size, const std::size_t align) noexcept {
        MemoryBlock block = TPrimary::allocate(size, align);
        if (block.ptr) {
            return block;
        } else {
            return TFallback::allocate(size, align);
        }
    }

    INLINE void deallocate(MemoryBlock& block) noexcept {
        if (TPrimary::owns(block)) {
            TPrimary::deallocate(block);
        } else {
            TFallback::deallocate(block);
        }
    }

    INLINE const TPrimary& primary() const {
        return *this;
    }

    INLINE TPrimary& primary() {
        return *this;
    }

    INLINE const TFallback& fallback() const {
        return *this;
    }

    INLINE TFallback& fallback() {
        return *this;
    }
};

/// \brief Compositor that uses one allocator for small allocations and another allocator for large
/// allocations.
template <std::size_t TLimit, typename TSmall, typename TLarge>
class Segregator : private TSmall, private TLarge {
public:
    INLINE MemoryBlock allocate(const std::size_t size, const std::size_t align) noexcept {
        if (size <= TLimit) {
            return TSmall::allocate(size, align);
        } else {
            return TLarge::allocate(size, align);
        }
    }

    INLINE void deallocate(MemoryBlock& block) noexcept {
        if (block.size <= TLimit) {
            TSmall::deallocate(block);
        } else {
            TLarge::deallocate(block);
        }
    }

    INLINE bool owns(const MemoryBlock& block) const noexcept {
        if (block.size <= TLimit) {
            return TSmall::owns(block);
        } else {
            return TLarge::owns(block);
        }
    }

    INLINE const TSmall& small() const {
        return *this;
    }

    INLINE const TLarge& large() const {
        return *this;
    }
};

/// \brief Helper allocator that keeps track on allocated memory.
template <typename TAllocator>
class TrackingAllocator : private TAllocator {
private:
    std::size_t memory = 0;

public:
    INLINE MemoryBlock allocate(const std::size_t size, const std::size_t align) noexcept {
        MemoryBlock block = TAllocator::allocate(size, align);
        if (block.ptr) {
            memory += size;
        }
        return block;
    }

    INLINE void deallocate(MemoryBlock& block) noexcept {
        if (block.ptr) {
            memory -= block.size;
        }
        TAllocator::deallocate(block);
    }

    INLINE bool owns(const MemoryBlock& block) const noexcept {
        return TAllocator::owns(block);
    }

    INLINE std::size_t allocated() const {
        return memory;
    }

    INLINE const TAllocator& underlying() const {
        return *this;
    }
};

NAMESPACE_SPH_END
