#pragma once

/// \file FlatMap.h
/// \brief Key-value associative container implemented as a sorted array
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/containers/Array.h"
#include "objects/containers/Tags.h"
#include "objects/wrappers/Optional.h"
#include <algorithm>

NAMESPACE_SPH_BEGIN

/// \brief Container of key-value pairs.
///
/// Elements are stored in an array sorted according to key. The value look-up is O(log(N)), while inserting
/// or deletion of elements is currently O(N).
template <typename TKey, typename TValue, typename TLess = std::less<TKey>>
class FlatMap : TLess, Noncopyable {
public:
    /// Element of the container.
    class Element {
        TKey k;
        TValue v;

    public:
        Element() = default;
        Element(const TKey& k, const TValue& v)
            : k(k)
            , v(v) {}
        Element(const TKey& k, TValue&& v)
            : k(k)
            , v(std::move(v)) {}

        INLINE const TKey& key() const {
            return k;
        }

        INLINE const TValue& value() const {
            return v;
        }

        INLINE TValue& value() {
            return v;
        }
    };

private:
    Array<Element> data;

public:
    FlatMap() = default;

    /// \brief Constructs the map from initializer list of elements.
    ///
    /// Tag specifies an optimization hint; it can be ELEMENTS_COMMON, ELEMENTS_UNIQUE, or
    /// ELEMENTS_SORTED_UNIQUE.
    template <typename Tag>
    FlatMap(Tag t, std::initializer_list<Element> list)
        : data(list) {
        this->create(t);
    }

    /// \brief Constructs the map from array of elements.
    ///
    /// Tag specifies an optimization hint; it can be ELEMENTS_COMMON, ELEMENTS_UNIQUE, or
    /// ELEMENTS_SORTED_UNIQUE.
    template <typename Tag>
    FlatMap(Tag t, Array<Element>&& values)
        : data(std::move(values)) {
        this->create(t);
    }

    /// \brief Returns a reference to the element, given its key.
    ///
    /// The element must exists in the map, checked by assert.
    INLINE TValue& operator[](const TKey& key) {
        Element* element = this->find(key);
        SPH_ASSERT(element);
        return element->value();
    }

    /// \brief Returns a reference to the element, given its key.
    ///
    /// The element must exists in the map, checked by assert.
    INLINE const TValue& operator[](const TKey& key) const {
        const Element* element = this->find(key);
        SPH_ASSERT(element);
        return element->value();
    }

    /// \brief Adds a new element into the map or sets new value of element with the same key.
    INLINE TValue& insert(const TKey& key, const TValue& value) {
        Element* element = this->find(key);
        if (!element) {
            return this->add(key, value);
        } else {
            element->value() = value;
            return element->value();
        }
    }

    /// \copydoc insert
    INLINE TValue& insert(const TKey& key, TValue&& value) {
        Element* element = this->find(key);
        if (!element) {
            return this->add(key, std::move(value));
        } else {
            element->value() = std::move(value);
            return element->value();
        }
    }

    /// \brief Removes element with given key from the map.
    ///
    /// The element must exists in the map, checked by assert.
    INLINE void remove(const TKey& key) {
        Element* element = this->find(key);
        SPH_ASSERT(element);
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
            return element->value();
        }
    }

    /// \brief Returns a reference to the value matching the given key, or NOTHING if no such value exists.
    INLINE Optional<const TValue&> tryGet(const TKey& key) const {
        const Element* element = this->find(key);
        if (!element) {
            return NOTHING;
        } else {
            return element->value();
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
    void create(ElementsCommonTag) {
        std::sort(data.begin(), data.end(), [this](const Element& e1, const Element& e2) {
            return less(e1.key(), e2.key());
        });
        auto end = std::unique(data.begin(), data.end(), [this](const Element& e1, const Element& e2) {
            return equal(e1.key(), e2.key());
        });
        data.resize(end - data.begin());
        SPH_ASSERT(this->keysSortedAndUnique());
    }

    void create(ElementsUniqueTag) {
        std::sort(data.begin(), data.end(), [this](const Element& e1, const Element& e2) {
            return less(e1.key(), e2.key());
        });
        SPH_ASSERT(this->keysSortedAndUnique());
    }

    void create(ElementsSortedUniqueTag) {
        SPH_ASSERT(this->keysSortedAndUnique());
    }

    bool keysSortedAndUnique() const {
        if (!std::is_sorted(data.begin(), data.end(), [this](const Element& e1, const Element& e2) {
                return less(e1.key(), e2.key());
            })) {
            return false;
        }

        for (Size i = 1; i < data.size(); ++i) {
            if (equal(data[i].key(), data[i - 1].key())) {
                return false;
            }
        }
        return true;
    }

    INLINE bool less(const TKey& key1, const TKey& key2) const {
        return TLess::operator()(key1, key2);
    }

    INLINE bool equal(const TKey& key1, const TKey& key2) const {
        return !less(key1, key2) && !less(key2, key1);
    }

    /// Returns a pointer to the element with given key or nullptr if no such element exists.
    INLINE Element* find(const TKey& key) {
        auto compare = [this](const Element& element, const TKey& key) { return less(element.key(), key); };
        auto iter = std::lower_bound(data.begin(), data.end(), key, compare);
        if (iter != data.end() && equal(iter->key(), key)) {
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
            SPH_ASSERT(!equal(data[mid].key(), key));
            if (less(data[mid].key(), key)) {
                from = mid + 1;
            } else {
                to = mid;
            }
        }
        data.insert(from, Element{ key, std::forward<T>(value) });
        return data[from].value();
    }
};


NAMESPACE_SPH_END
