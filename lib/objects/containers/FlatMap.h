#pragma once

/// \file FlatMap.h
/// \brief Key-value associative container implemented as a sorted array
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "objects/containers/Array.h"
#include "objects/wrappers/Optional.h"
#include <algorithm>

NAMESPACE_SPH_BEGIN

/// \brief Comparator of keys, returning true if first key has lower value than the second one.
///
/// The function can be specialized for types without < operator. The relation has to have the properties of
/// order, though, namely transitive property (a < b && b < c => a < c).
template <typename TKey>
INLINE constexpr bool compare(const TKey& key1, const TKey& key2) {
    return key1 < key2;
}

/// \brief Container of key-value pairs.
///
/// Elements are stored in an array sorted according to key. The value look-up is O(log(N)), while inserting
/// or deletion of elements is currently O(N).
template <typename TKey, typename TValue>
class FlatMap : public Noncopyable {
public:
    /// Element of the container.
    struct Element {
        /// \todo we definitely don't want to expose mutable key when iterating. Possibly create two structs
        /// -- PrivateElement and Element -- and just reinterpret_cast ? (kinda crazy)
        TKey key;
        TValue value;
    };

private:
    Array<Element> data;

public:
    FlatMap() = default;

    /// \brief Constructs the map fromm initializer list of elements.
    ///
    /// Elements do not have to be sorted in the initializer list, the keys of the elements have to be unique,
    /// i.e. each key has to be present at most once. This is checked by assert.
    FlatMap(std::initializer_list<Element> list)
        : data(list) {
        std::sort(data.begin(), data.end(), [](Element& e1, Element& e2) { return compare(e1.key, e2.key); });
    }

    /// \brief Returns a reference to the element, given its key.
    ///
    /// The element must exists in the map, checked by assert.
    INLINE TValue& operator[](const TKey& key) {
        Element* element = this->find(key);
        ASSERT(element);
        return element->value;
    }

    /// \brief Returns a reference to the element, given its key.
    ///
    /// The element must exists in the map, checked by assert.
    INLINE const TValue& operator[](const TKey& key) const {
        const Element* element = this->find(key);
        ASSERT(element);
        return element->value;
    }

    /// \brief Adds a new element into the map or sets new value of element with the same key.
    INLINE TValue& insert(const TKey& key, const TValue& value) {
        Element* element = this->find(key);
        if (!element) {
            return this->add(key, value);
        } else {
            element->value = value;
            return element->value;
        }
    }

    /// \copydoc insert
    INLINE TValue& insert(const TKey& key, TValue&& value) {
        Element* element = this->find(key);
        if (!element) {
            return this->add(key, std::move(value));
        } else {
            element->value = std::move(value);
            return element->value;
        }
    }

    /// \brief Removes element with given key from the map.
    ///
    /// The element must exists in the map, checked by assert.
    INLINE void remove(const TKey& key) {
        Element* element = this->find(key);
        ASSERT(element);
        const Size index = element - &data[0];
        data.remove(index);
    }

    /// \brief Removes element with given key if present, otherwise it does nothing.
    ///
    /// \return True if the element was removed, false otherwise.
    INLINE bool tryRemove(const TKey& key) {
        Element* element = this->find(key);
        if (!element) {
            return false;
        } else {
            const Size index = element - &data[0];
            data.remove(index);
            return true;
        }
    }

    /// \brief Removes all elements from the map.
    INLINE void clear() {
        data.clear();
    }

    /// \brief Returns a reference to the value matching the given key, or NOTHING if no such value exists.
    ///
    /// Safe alternative to operator[].
    INLINE Optional<TValue&> tryGet(const TKey& key) {
        Element* element = this->find(key);
        if (!element) {
            return NOTHING;
        } else {
            return element->value;
        }
    }

    /// \brief Returns a reference to the value matching the given key, or NOTHING if no such value exists.
    INLINE Optional<const TValue&> tryGet(const TKey& key) const {
        const Element* element = this->find(key);
        if (!element) {
            return NOTHING;
        } else {
            return element->value;
        }
    }

    /// \brief Returns true if the map contains element of given key.
    ///
    /// Equivalent to bool(tryGet(key)).
    INLINE bool contains(const TKey& key) const {
        return this->find(key) != nullptr;
    }

    /// Returns the number of elements in the map.
    INLINE Size size() const {
        return data.size();
    }

    /// Returns true if the map contains no elements, false otherwise.
    INLINE Size empty() const {
        return data.empty();
    }

    /// \brief Returns the iterator pointing to the first element.
    INLINE Iterator<Element> begin() {
        return data.begin();
    }

    /// \brief Returns the iterator pointing to the first element.
    INLINE Iterator<const Element> begin() const {
        return data.begin();
    }

    /// \brief Returns the iterator pointing to the one-past-last element.
    INLINE Iterator<Element> end() {
        return data.end();
    }

    /// \brief Returns the iterator pointing to the one-past-last element.
    INLINE Iterator<const Element> end() const {
        return data.end();
    }

    INLINE operator ArrayView<Element>() {
        return data;
    }

    INLINE operator ArrayView<const Element>() const {
        return data;
    }

    FlatMap clone() const {
        FlatMap cloned;
        cloned.data = data.clone();
        return cloned;
    }

private:
    /// Returns a pointer to the element with given key or nullptr if no such element exists.
    INLINE Element* find(const TKey& key) {
        auto compare = [](const Element& element, const TKey& key) { return element.key < key; };
        auto iter = std::lower_bound(data.begin(), data.end(), key, compare);
        if (iter != data.end() && iter->key == key) {
            return &*iter;
        } else {
            return nullptr;
        }
    }

    INLINE const Element* find(const TKey& key) const {
        return const_cast<FlatMap*>(this)->find(key);
    }

    /// Adds new element into the map, assuming no element with the same key exists.
    template <typename T>
    INLINE TValue& add(const TKey& key, T&& value) {
        Size from = 0;
        Size to = data.size();
        Size mid = Size(-1);

        while (from < to && from != mid) {
            mid = (from + to) / 2;
            ASSERT(data[mid].key != key);
            if (compare(data[mid].key, key)) {
                from = mid + 1;
            } else {
                to = mid;
            }
        }
        /// \todo add insert into Array
        data.resize(data.size() + 1);
        std::move_backward(data.begin() + from, data.end() - 1, data.end());
        data[from] = Element{ key, std::forward<T>(value) };
        return data[from].value;
    }
};


NAMESPACE_SPH_END
