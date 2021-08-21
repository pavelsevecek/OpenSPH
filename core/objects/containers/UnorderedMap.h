#pragma once

/// \file UnorderedMap.h
/// \brief Key-value associative container
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/containers/Array.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

template <typename TKey, typename TValue>
class UnorderedMap : public Noncopyable {
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

        const TKey& key() const {
            return k;
        }
        const TValue& value() const {
            return v;
        }
        TValue& value() {
            return v;
        }
    };

private:
    Array<Element> data;

public:
    UnorderedMap() = default;

    /// \brief Constructs the map fromm initializer list of elements.
    UnorderedMap(std::initializer_list<Element> list)
        : data(list) {}

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

    /// \copydoc insert(const TKey& key, const TValue& value)
    INLINE TValue& insert(const TKey& key, TValue&& value) {
        Element* element = this->find(key);
        if (!element) {
            return this->add(key, std::move(value));
        } else {
            element->value() = std::move(value);
            return element->value();
        }
    }

    /// \brief Inserts a new element into the given position in the map or sets new value of element with the
    /// same key.
    INLINE TValue& insert(const TKey& key, const Size position, const TValue& value) {
        Element* element = this->find(key);
        if (!element) {
            return this->add(key, position, value);
        } else {
            element->value() = value;
            return element->value();
        }
    }

    /// \copydoc insert(const TKey& key, const Size position, const TValue& value) {
    INLINE TValue& insert(const TKey& key, const Size position, TValue&& value) {
        Element* element = this->find(key);
        if (!element) {
            return this->add(key, position, std::move(value));
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
        const Size index = Size(element - &data[0]);
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

    UnorderedMap clone() const {
        UnorderedMap cloned;
        cloned.data = data.clone();
        return cloned;
    }

private:
    /// Returns a pointer to the element with given key or nullptr if no such element exists.
    INLINE Element* find(const TKey& key) {
        for (Element& element : data) {
            if (element.key() == key) {
                return &element;
            }
        }
        return nullptr;
    }

    INLINE const Element* find(const TKey& key) const {
        return const_cast<UnorderedMap*>(this)->find(key);
    }

    /// Adds new element into the map, assuming no element with the same key exists.
    template <typename T>
    INLINE TValue& add(const TKey& key, T&& value) {
        data.push(Element{ key, std::forward<T>(value) });
        return data.back().value();
    }

    /// Inserts new element into given position
    template <typename T>
    INLINE TValue& add(const TKey& key, const Size position, T&& value) {
        data.insert(position, Element{ key, std::forward<T>(value) });
        return data.back().value();
    }
};


NAMESPACE_SPH_END
