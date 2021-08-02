#pragma once

#include "objects/containers/BasicAllocators.h"

NAMESPACE_SPH_BEGIN

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
        resource = &other;
    }

    INLINE MemoryBlock allocate(const std::size_t size, const Size align) noexcept {
        if (resource) {
            return resource->allocate(size, align);
        } else {
            return MemoryBlock::EMPTY();
        }
    }

    INLINE void deallocate(MemoryBlock& UNUSED(block)) noexcept {}

    INLINE bool owns(const MemoryBlock& block) const noexcept {
        if (resource) {
            return resource->owns(block);
        } else {
            return false;
        }
    }
};

/// \brief Simple memory resource with pre-allocated contiguous memory buffer.
template <typename TAllocator>
class MonotonicMemoryResource : private TAllocator {
    MemoryBlock resource;
    std::size_t position = 0;

public:
    MonotonicMemoryResource(const std::size_t size, const std::size_t align) {
        resource = TAllocator::allocate(size, align);
    }

    ~MonotonicMemoryResource() {
        TAllocator::deallocate(resource);
    }

    INLINE MemoryBlock allocate(const std::size_t size, const std::size_t align) noexcept {
        if (position + size <= resource.size) {
            MemoryBlock block;
            block.ptr = roundToAlignment((uint8_t*)resource.ptr + position, align);
            SPH_ASSERT(isAligned(block.ptr, align));
            block.size = size;
            SPH_ASSERT(isAligned(block.size, align));
            position = ((uint8_t*)block.ptr + block.size) - (uint8_t*)resource.ptr;
            return block;
        } else {
            return MemoryBlock::EMPTY();
        }
    }

    INLINE bool owns(const MemoryBlock& block) const noexcept {
        return block.ptr >= resource.ptr && block.ptr < (uint8_t*)resource.ptr + resource.size;
    }
};

template <typename TAllocator>
class FreeListAllocator : private TAllocator {
private:
    struct Node {
        MemoryBlock block;
        Node* next = nullptr;
    };

    Node* list = nullptr;

public:
    ~FreeListAllocator() {
        while (list) {
            TAllocator::deallocate(list->block);
            Node* next = list->next;
            allocatorDelete(this->underlying(), list);
            list = next;
        }
    }

    INLINE MemoryBlock allocate(const std::size_t size, const Size align) noexcept {
        if (list != nullptr) {
            SPH_ASSERT(list->block.ptr);
            SPH_ASSERT(list->block.size == size); // could be generalized if needed
            MemoryBlock block = list->block;
            list = list->next;
            return block;
        } else {
            return TAllocator::allocate(size, align);
        }
    }

    INLINE void deallocate(MemoryBlock& block) noexcept {
        SPH_ASSERT(block.ptr);
        Node* node = allocatorNew<Node>(this->underlying());
        SPH_ASSERT(node);
        node->block = block;
        node->next = list;
        list = node;
    }

    // only for debug purposes
    Size getListSize() const {
        Size size = 0;
        Node* ptr = list;
        while (ptr) {
            size++;
            ptr = ptr->next;
        }
        return size;
    }

    const TAllocator& underlying() const {
        return *this;
    }

    TAllocator& underlying() {
        return *this;
    }
};

NAMESPACE_SPH_END
