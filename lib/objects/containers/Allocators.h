#pragma once

/// \file Allocators.h
/// \brief Allocators used by containers
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "common/Assert.h"

NAMESPACE_SPH_BEGIN

struct Block {
    void* ptr;
    Size size;

    INLINE static Block EMPTY() {
        return { nullptr, 0 };
    }
};

class Mallocator {
public:
    INLINE Block allocate(const Size size) noexcept {
        Block block;
        block.ptr = malloc(size);
        if (!block.ptr) {
            block.size = 0;
        } else {
            block.size = size;
        }
        return block;
    }

    INLINE void deallocate(Block& block) noexcept {
        free(block.ptr);
        block.ptr = nullptr;
    }
};


template <Size TSize, Size TAlign = 16>
class StackAllocator : public Immovable, Local {
private:
    alignas(TAlign) char data[TSize];

    /// Current position in the buffer
    char* pos = data;

public:
    INLINE Block allocate(const Size size) noexcept {
        ASSERT(size > 0);

        const Size actSize = roundToAlignment(size);
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
        if ((char*)block.ptr + block.size == pos) {
            pos = static_cast<char*>(block.ptr);
        }
        block = Block::EMPTY();
    }


private:
    INLINE constexpr static Size roundToAlignment(const Size value) noexcept {
        const Size remainder = value % TAlign;
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
    INLINE Block allocate(const Size size) noexcept {
        Block block = TPrimary::allocate(size);
        if (block.ptr) {
            return block;
        } else {
            return TFallback::allocate(size);
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

template <Size TLimit, typename TSmall, typename TLarge>
class Segregator : private TSmall, private TLarge {
public:
    INLINE Block allocate(const Size size) noexcept {
        if (size <= TLimit) {
            return TSmall::allocate(size);
        } else {
            return TLarge::allocate(size);
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

NAMESPACE_SPH_END
