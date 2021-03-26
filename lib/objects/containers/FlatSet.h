#pragma once

/// \file FlatSet.h
/// \brief Container storing sorted unique values.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "objects/containers/Array.h"
#include "objects/wrappers/Optional.h"
#include <algorithm>

NAMESPACE_SPH_BEGIN

template <typename T, typename TLess = std::less<T>>
class FlatSet : TLess {
private:
    Array<T> data;

public:
    FlatSet() = default;

    FlatSet(std::initializer_list<T> list) {
        for (const T& value : list) {
            this->insert(value);
        }
    }

    INLINE Size size() const {
        return data.size();
    }

    INLINE bool empty() const {
        return data.empty();
    }

    INLINE T& operator[](const Size idx) {
        return data[idx];
    }

    INLINE const T& operator[](const Size idx) const {
        return data[idx];
    }

    template <typename U>
    void insert(U&& value) {
        Size from = 0;
        Size to = data.size();
        Size mid = Size(-1);

        while (from < to && from != mid) {
            mid = (from + to) / 2;
            if (less(data[mid], value)) {
                from = mid + 1;
            } else if (less(value, data[mid])) {
                to = mid;
            } else {
                // found the same value, skip
                return;
            }
        }
        data.insert(from, std::forward<U>(value));
    }

    template <typename TIter>
    void insert(TIter first, TIter last) {
        data.insert(data.size(), first, last);
        std::sort(data.begin(), data.end(), TLess{});
        data.remove(std::unique(data.begin(), data.end()), data.end());
    }

    void reserve(const Size capacity) {
        data.reserve(capacity);
    }

    Iterator<T> find(const T& value) {
        auto iter = std::lower_bound(data.begin(), data.end(), value);
        if (iter != data.end() && *iter == value) {
            return iter;
        } else {
            return data.end();
        }
    }

    Iterator<const T> find(const T& value) const {
        return const_cast<FlatSet*>(this)->find(value);
    }

    bool contains(const T& value) {
        return this->find(value) != this->end();
    }

    Iterator<T> erase(Iterator<T> pos) {
        const Size idx = pos - data.begin();
        data.remove(idx);
        return data.begin() + idx;
    }

    void clear() {
        data.clear();
    }

    Iterator<T> begin() {
        return data.begin();
    }

    Iterator<const T> begin() const {
        return data.begin();
    }

    Iterator<T> end() {
        return data.end();
    }

    Iterator<const T> end() const {
        return data.end();
    }

    operator ArrayView<T>() {
        return data;
    }

    operator ArrayView<const T>() const {
        return data;
    }

    const Array<T>& array() const& {
        return data;
    }

    Array<T> array() && {
        return std::move(data);
    }

private:
    INLINE bool less(const T& t1, const T& t2) const {
        return TLess::operator()(t1, t2);
    }
};

NAMESPACE_SPH_END
