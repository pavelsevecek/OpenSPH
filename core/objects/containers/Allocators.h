#pragma once

/// \file Allocators.h
/// \brief Allocators used by containers
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "common/Assert.h"
#include <mm_malloc.h>

NAMESPACE_SPH_BEGIN

struct Block {
    void* ptr;
    std::size_t size;

    INLINE static Block EMPTY() {
        return { nullptr, 0 };
    }
};

class Mallocator {
public:
    INLINE Block allocate(const std::size_t size, const std::size_t align) noexcept {
        Block block;
        block.ptr = _mm_malloc(size, align);
        if (!block.ptr) {
            block.size = 0;
        } else {
            block.size = size;
        }
        return block;
    }

    INLINE void deallocate(Block& block) noexcept {
        _mm_free(block.ptr);
        block.ptr = nullptr;
    }
};


template <std::size_t TSize, Size TAlign = 16>
class StackAllocator : public Immovable, Local {
private:
    alignas(TAlign) char data[TSize];

    /// Current position in the buffer
    uint8_t* pos = data;

public:
    INLINE Block allocate(const std::size_t size, const std::size_t UNUSED(align)) noexcept {
        SPH_ASSERT(size > 0);

        const std::size_t actSize = roundToAlignment(size);
        if (pos - data + actSize > TSize) {
            return Block::EMPTY();
        }

        void* ptr = pos;
        pos += size;
        return { ptr, size };
    }

    INLINE void deallocate(Block& block) noexcept {
        if (!block.ptr) {
            return;
        }
        if ((uint8_t*)block.ptr + block.size == pos) {
            pos = static_cast<uint8_t*>(block.ptr);
        }
        block = Block::EMPTY();
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
    INLINE Block allocate(const std::size_t size, const std::size_t align) noexcept {
        Block block = TPrimary::allocate(size, align);
        if (block.ptr) {
            return block;
        } else {
            return TFallback::allocate(size, align);
        }
    }

    INLINE void deallocate(Block& block) noexcept {
        if (TPrimary::owns(block)) {
            TPrimary::deallocate(block);
        } else {
            TFallback::deallocate(block);
        }
    }
};

template <std::size_t TLimit, typename TSmall, typename TLarge>
class Segregator : private TSmall, private TLarge {
public:
    INLINE Block allocate(const std::size_t size, const std::size_t align) noexcept {
        if (size <= TLimit) {
            return TSmall::allocate(size, align);
        } else {
            return TLarge::allocate(size, align);
        }
    }

    INLINE void deallocate(Block& block) noexcept {
        if (block.size <= TLimit) {
            TSmall::deallocate(block);
        } else {
            TLarge::deallocate(block);
        }
    }
};

class MemoryResourceAllocator {
private:
    Block resource;
    std::size_t allocated = 0;

public:
    explicit MemoryResourceAllocator(const Block& resource)
        : resource(resource) {}

    INLINE Block allocate(const std::size_t size, const Size UNUSED(align)) noexcept {
        if (allocated + size <= resource.size) {
            Block block;
            block.ptr = (uint8_t*)resource.ptr + allocated;
            block.size = size;
            allocated += size;
            return block;
        } else {
            return Block::EMPTY();
        }
    }

    INLINE void deallocate(Block& UNUSED(block)) noexcept {}
};

template <typename TAllocator>
class MemoryResource : public TAllocator {
    Block resource;

public:
    explicit MemoryResource(const std::size_t size, std::size_t align) {
        resource = TAllocator::allocate(size, align);
    }

    ~MemoryResource() {
        TAllocator::deallocate(resource);
    }

    Block block() const {
        return resource;
    }
};

NAMESPACE_SPH_END
