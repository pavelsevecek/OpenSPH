#pragma once

/// \file Map.h
/// \brief Key-value associative container
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/containers/Array.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

/// \brief Comparator of keys, returning true if first key has lower value than the second one.
///
/// The function can be specialized for types without < operator. The relation has to have the properties of
/// order, though, namely transitive property (a < b && b < c => a < c).
template <typename TKey>
INLINE constexpr bool compare(const TKey& key1, const TKey& key2) {
    return key1 < key2;
}

/// Hint used to optimize the map queries
enum class MapOptimization {
    LARGE, ///< Optimize the map for large numbers of elements (generally larger than 10)
    SMALL, ///< Optimize the map for small numbers of elements (generally smaller than 10)
};

/// \brief Container of key-value pairs.
///
/// Elements are stored in an array sorted according to key. The value look-up is O(log(N)), while inserting
/// or deletion of elements is currently O(N).
template <typename TKey, typename TValue, MapOptimization Optimize = MapOptimization::LARGE>
class Map : public Noncopyable {
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
    Map() = default;

    /// \brief Constructs the map fromm initializer list of elements.
    ///
    /// Elements do not have to be sorted in the initializer list, the keys of the elements have to be unique,
    /// i.e. each key has to be present at most once. This is checked by assert.
    Map(std::initializer_list<Element> list)
        : data(list) {
        std::sort(data.begin(), data.end(), [](Element& e1, Element& e2) {
            ASSERT(e1.key != e2.key); // keys must be unique
            return compare(e1.key, e2.key);
        });
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
        Element* element = this->find(key);
        ASSERT(element);
        return element->value;
    }

    /// \brief Adds a new element into the map or sets new value of element with the same key.
    INLINE void insert(const TKey& key, const TValue& value) {
        Element* element = this->find(key);
        if (!element) {
            this->add(key, value);
        } else {
            element->value = value;
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
        }
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

private:
    /// Returns a pointer to the element with given key or nullptr if no such element exists.
    INLINE Element* find(const TKey& key) {
        if (Optimize == MapOptimization::LARGE) {
            Size from = 0;
            Size to = data.size();
            Size mid = Size(-1);

            while (from < to && from != mid) {
                mid = (from + to) / 2;
                if (compare(data[mid].key, key)) {
                    from = mid + 1;
                } else if (data[mid].key == key) {
                    return &data[mid];
                } else {
                    to = mid;
                }
            }
            return nullptr;
        } else {
            if (data.empty()) {
                return nullptr;
            }
            const Size mid = data.size() / 2;
            if (compare(data[mid].key, key)) {
                for (Size i = mid + 1; i < data.size(); ++i) {
                    if (data[i].key == key) {
                        return &data[i];
                    }
                }
            } else {
                for (Size i = 0; i <= mid; ++i) {
                    if (data[i].key == key) {
                        return &data[i];
                    }
                }
            }
            return nullptr;
        }
    }

    INLINE const Element* find(const TKey& key) const {
        return const_cast<Map*>(this)->find(key);
    }

    /// Adds new element into the map, assuming no element with the same key exists.
    INLINE void add(const TKey& key, const TValue& value) {
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
        data[from] = Element{ key, value };
    }
};


/// Alias for the map optimized for small number of elements
template <typename TKey, typename TValue>
using SmallMap = Map<TKey, TValue, MapOptimization::SMALL>;

NAMESPACE_SPH_END