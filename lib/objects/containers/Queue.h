#pragma once

/// \file Queue.h
/// \brief Double-ended queue
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "math/MathUtils.h"
#include "objects/containers/ArrayView.h"
#include <mm_malloc.h>

NAMESPACE_SPH_BEGIN

/// \brief Container allowing to add and remove elements from both ends.
template <typename T, typename TCounter = Size>
class Queue : public Noncopyable {
private:
    using StorageType = typename WrapReferenceType<T>::Type;

    /// Allocated data (not necessarily constructed)
    StorageType* data = nullptr;

    /// First constructed element in the array
    TCounter first = 0;

    /// One-past-last constructed element in the array
    TCounter last = 0;

    /// Number of allocated elements
    TCounter maxSize = 0;

public:
    /// \brief Constructs an empty queue.
    Queue() = default;

    /// \brief Constructs a queue with given number of elements.
    ///
    /// The elements are default-constructed.
    Queue(const TCounter size) {
        this->alloc(size, 0, 0);
        for (TCounter i = 0; i < maxSize; ++i) {
            new (data + i) StorageType();
        }
        last = maxSize;
    }

    /// \brief Constructs a queue from initializer list.
    ///
    /// The elements are created using copy constructor.
    Queue(std::initializer_list<StorageType> list) {
        this->alloc(list.size(), 0, 0);
        TCounter i = 0;
        for (auto& l : list) {
            new (data + i) StorageType(l);
            i++;
        }
        last = maxSize;
    }

    Queue(Queue&& other)
        : data(other.data)
        , first(other.first)
        , last(other.last)
        , maxSize(other.maxSize) {
        other.data = nullptr;
        other.first = other.last = other.maxSize = 0;
    }

    ~Queue() {
        for (TCounter i = first; i < last; ++i) {
            data[i].~StorageType();
        }
        if (data) {
            _mm_free(data);
            data = nullptr;
        }
    }

    Queue& operator=(Queue&& other) {
        std::swap(data, other.data);
        std::swap(first, other.first);
        std::swap(last, other.last);
        std::swap(maxSize, other.maxSize);
        return *this;
    }

    /// \brief Returns a reference to idx-th element in the queue.
    ///
    /// If the index is out of bounds, an assert is issued.
    INLINE T& operator[](const TCounter idx) {
        ASSERT(idx < this->size());
        return data[first + idx];
    }

    /// \brief Returns a const reference to idx-th element in the queue
    ///
    /// If the index is out of bounds, an assert is issued.
    INLINE const T& operator[](const TCounter idx) const {
        ASSERT(idx < this->size());
        return data[first + idx];
    }

    /// \brief Returns a reference to the first element in the queue
    ///
    /// If the queue contains no elements, an assert is issued.
    INLINE T& front() {
        ASSERT(!this->empty());
        return data[first];
    }

    /// \brief Returns a const reference to the first element in the queue
    ///
    /// If the queue contains no elements, an assert is issued.
    INLINE const T& front() const {
        ASSERT(!this->empty());
        return data[first];
    }

    /// \brief Returns a reference to the last element in the queue
    ///
    /// If the queue contains no elements, an assert is issued.
    INLINE T& back() {
        ASSERT(!this->empty());
        return data[last - 1];
    }

    /// \brief Returns a const reference to the last element in the queue
    ///
    /// If the queue contains no elements, an assert is issued.
    INLINE const T& back() const {
        ASSERT(!this->empty());
        return data[last - 1];
    }

    /// \brief Returns the number of elements in the queue.
    ///
    /// Note that the number of allocated elements may be bigger.
    INLINE Size size() const {
        return last - first;
    }

    /// \brief Returns true if the queue contains no elements.
    ///
    /// Note that this does not mean no memory is allocated.
    INLINE bool empty() const {
        return last == first;
    }

    /// \brief Adds a new element to the front of the queue.
    ///
    /// A new element in the queue is default-constructed and the given value is assigned using copy operator.
    void pushFront(const T& value) {
        if (first == 0) {
            // no place for new elements, needs to resize
            this->reserveFront(1);
        }
        ASSERT(first > 0);
        --first;
        new (data + first) StorageType();
        data[first] = value;
    }

    /// \brief Adds a new element to the back of the queue.
    ///
    /// A new element in the queue is default-constructed and the given value is assigned using copy operator.
    void pushBack(const T& value) {
        if (last == maxSize) {
            // no place for new elements, needs to resize
            this->reserveBack(1);
        }
        ASSERT(last < maxSize);
        ++last;
        new (data + last - 1) StorageType();
        data[last - 1] = value;
    }

    /// \brief Removes an element from the front of the queue.
    ///
    /// \return The value of the removed element.
    T popFront() {
        ASSERT(!this->empty());
        T value = this->front();
        data[first].~StorageType();
        ++first;
        return value;
    }

    /// \brief Removes an element from the back of the queue.
    ///
    /// \return The value of the removed element.
    T popBack() {
        ASSERT(!this->empty());
        T value = this->back();
        data[last - 1].~StorageType();
        --last;
        return value;
    }

    /// \brief Removes all elements from the queue.
    ///
    /// Note that this does not deallocate the queue. This is done in destructor.
    void clear() {
        for (TCounter i = first; i < last; ++i) {
            data[i].~StorageType();
        }
        last = first;
    }

    /// \brief Returns an iterator pointing to the first element of the queue.
    INLINE Iterator<StorageType, TCounter> begin() {
        return Iterator<StorageType, TCounter>(data + first, data + first, data + last);
    }

    /// \brief Returns a const iterator pointing to the first element of the queue.
    INLINE Iterator<const StorageType, TCounter> begin() const {
        return Iterator<const StorageType, TCounter>(data + first, data + first, data + last);
    }

    /// \brief Returns an iterator pointing to the one-past-last element of the queue.
    INLINE Iterator<StorageType, TCounter> end() {
        return Iterator<StorageType, TCounter>(data + last, data + first, data + last);
    }

    /// \brief Returns a const iterator pointing to the one-past-last element of the queue.
    INLINE Iterator<const StorageType, TCounter> end() const {
        return Iterator<const StorageType, TCounter>(data + last, data + first, data + last);
    }

    /// \brief Implicit conversion to arrayview.
    INLINE operator ArrayView<T, TCounter>() {
        return ArrayView<T, TCounter>(data + first, this->size());
    }

    /// \brief Implicit conversion to const arrayview.
    INLINE operator ArrayView<const T, TCounter>() const {
        return ArrayView<const T, TCounter>(data + first, this->size());
    }


private:
    /// Allocates memory given extra space for element in the front and in the back.
    /// Does not construct any elements
    void alloc(const TCounter size, const TCounter extraFront, const TCounter extraBack) {
        maxSize = size + extraFront + extraBack;
        first = last = extraFront;

        data = (StorageType*)_mm_malloc(maxSize * sizeof(StorageType), alignof(StorageType));
    }

    /// If there is currently less than num free elements in the front, reallocates the queue, adding at least
    /// num free elements to the front.
    void reserveFront(const TCounter num) {
        if (num > first) {
            Queue newQueue;
            newQueue.alloc(this->size(), max(num, this->size()), min(maxSize - last, this->size()));
            this->moveElements(std::move(newQueue));
        }
        ASSERT(num <= first);
    }

    /// If there is currently less than num free elements in the back, reallocates the queue, adding at least
    /// num free elements to the back.
    void reserveBack(const TCounter num) {
        if (num > maxSize - last) {
            Queue newQueue;
            newQueue.alloc(this->size(), min(first, this->size()), max(num, this->size()));
            this->moveElements(std::move(newQueue));
        }
        ASSERT(num <= maxSize - last);
    }

    /// Moves all elements from this queue to a new one, assuming enough memory is allocated.
    void moveElements(Queue&& newQueue) {
        newQueue.last = newQueue.first + this->size();
        ASSERT(newQueue.last <= newQueue.maxSize);

        // construct all element in the new queue using move constructor
        for (TCounter i = 0; i < this->size(); ++i) {
            new (newQueue.data + newQueue.first + i) StorageType(std::move(data[i + first]));
        }
        // replace queues
        *this = std::move(newQueue);
    }
};

NAMESPACE_SPH_END
