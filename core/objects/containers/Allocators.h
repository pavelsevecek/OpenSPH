#pragma once

/// \file Allocators.h
/// \brief Allocators used by containers
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "common/Assert.h"
#include <memory>
#include <mm_malloc.h>

NAMESPACE_SPH_BEGIN

struct MemoryBlock {
    void* ptr;
    std::size_t size;

    INLINE static MemoryBlock EMPTY() {
        return { nullptr, 0 };
    }
};

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

        const std::size_t actSize = roundToAlignment(size);
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

private:
    INLINE constexpr static std::size_t roundToAlignment(const std::size_t value) noexcept {
        const std::size_t remainder = value % TAlign;
        return value + ((remainder == 0) ? 0 : (TAlign - remainder));
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

    INLINE const TFallback& fallback() const {
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

/// \brief Allocator that obtains memory blocks from given memory resource.
///
/// Allocator does not own the resource, therefore the resource must exist as long as any object that uses the
/// allocator. Allocator cannot deallocate memory.
template <typename TResource>
class MemoryResourceAllocator {
private:
    TResource* resource = nullptr;

public:
    INLINE void bind(TResource& other) {
        resource = std::addressof(other);
    }

    INLINE MemoryBlock allocate(const std::size_t size, const Size align) noexcept {
        return resource->allocate(size, align);
    }

    INLINE void deallocate(MemoryBlock& UNUSED(block)) noexcept {}

    INLINE bool owns(const MemoryBlock& block) const noexcept {
        return resource->owns(block);
    }
};

/// \brief Simple memory resource with pre-allocated contiguous memory buffer.
template <typename TAllocator>
class MonotonicMemoryResource : public TAllocator {
    MemoryBlock resource;
    std::size_t position = 0;

public:
    MonotonicMemoryResource(const std::size_t size, const std::size_t align) {
        resource = TAllocator::allocate(size, align);
    }

    ~MonotonicMemoryResource() {
        TAllocator::deallocate(resource);
    }

    INLINE MemoryBlock allocate(const std::size_t size, const std::size_t UNUSED(align)) noexcept {
        if (position + size <= resource.size) {
            MemoryBlock block;
            block.ptr = (uint8_t*)resource.ptr + position;
            block.size = size;
            position += size;
            return block;
        } else {
            return MemoryBlock::EMPTY();
        }
    }

    INLINE bool owns(const MemoryBlock& block) const noexcept {
        return block.ptr >= resource.ptr && block.ptr < resource.ptr + resource.size;
    }
};

NAMESPACE_SPH_END
