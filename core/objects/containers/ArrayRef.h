#pragma once

#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

enum class RefEnum {
    /// Weak reference; the object holds only a pointer and the data must be owned by another Array or some
    /// container. Once the owning container is reallocated or deleted, the reference is invalidated.
    WEAK,

    /// Strong reference, the data are copied into the object and the object is now the owner. No external
    /// buffer is referenced. The data are always valid.
    STRONG,
};

template <typename T>
class ArrayRef {
private:
    /// View referencing either the holder array or some external buffer
    ArrayView<T> ref;

    /// Holder of data, or empty if external buffer is used.
    Array<std::remove_const_t<T>> holder;

public:
    ArrayRef() = default;

    ArrayRef(ArrayView<T> data, const RefEnum type) {
        if (type == RefEnum::WEAK) {
            ref = data;
        } else {
            holder.reserve(data.size());
            for (Size i = 0; i < data.size(); ++i) {
                holder.emplaceBack(data[i]);
            }
            ref = holder;
        }
    }

    ArrayRef(ArrayRef&& other)
        : holder(std::move(other.holder)) {
        this->reset(other);
    }

    ArrayRef& operator=(ArrayRef&& other) {
        holder = std::move(other.holder);
        this->reset(other);
        return *this;
    }

    INLINE T& operator[](const Size idx) {
        return ref[idx];
    }

    INLINE const T& operator[](const Size idx) const {
        return ref[idx];
    }

    INLINE Size size() const {
        return ref.size();
    }

    INLINE bool empty() const {
        return size() == 0;
    }

    /// \brief Returns true if the referenced data are held by the object.
    INLINE bool owns() const {
        return !holder.empty();
    }

    /// \brief Copies the referenced buffer into the internal storage, if not already owning the buffer.
    ///
    /// The external referenced buffer is not modified. It can be later safely modified without invalidating
    /// data stored in this object.
    void seize() {
        if (!owns() && !empty()) {
            holder.reserve(ref.size());
            for (Size i = 0; i < ref.size(); ++i) {
                holder.emplaceBack(ref[i]);
            }
            ref = holder;
        }
    }

    Iterator<T> begin() {
        return ref.begin();
    }

    Iterator<const T> begin() const {
        return ref.begin();
    }

    Iterator<T> end() {
        return ref.end();
    }

    Iterator<const T> end() const {
        return ref.end();
    }

    operator ArrayView<T>() {
        return ref;
    }

    operator ArrayView<const T>() const {
        return ref;
    }

private:
    /// Sets the view to reference the internal holder or copies the view of other object, depending on
    /// whether the other object owns (owned) the reference. The view of the moved object is also reset to set
    /// the object into valid state.
    void reset(ArrayRef& other) {
        if (this->owns()) {
            // set the reference to our buffer
            ref = holder;
            // we moved the buffer from other, so we must also reset the (invalidated) view
            other.ref = nullptr;
        } else {
            // copy the reference (no need to change anything in the moved object)
            ref = other.ref;
        }
    }
};

template <typename T>
ArrayRef<T> makeArrayRef(Array<T>& data, const RefEnum type) {
    return ArrayRef<T>(data, type);
}

template <typename T>
ArrayRef<const T> makeArrayRef(const Array<T>& data, const RefEnum type) {
    return ArrayRef<const T>(data, type);
}

template <typename T>
ArrayRef<T> makeArrayRef(Array<T>&& data) {
    // we never want to hold a weak reference to temporary data
    return ArrayRef<T>(std::move(data), RefEnum::STRONG);
}

NAMESPACE_SPH_END
