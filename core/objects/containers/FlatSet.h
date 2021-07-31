#pragma once

/// \file FlatSet.h
/// \brief Container storing sorted unique values.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/containers/Array.h"
#include "objects/containers/Tags.h"
#include "objects/wrappers/Optional.h"
#include <algorithm>

NAMESPACE_SPH_BEGIN

template <typename T, typename TLess = std::less<T>>
class FlatSet : TLess, Noncopyable {
private:
    Array<T> data;

public:
    FlatSet() = default;

    template <typename Tag>
    FlatSet(Tag t, std::initializer_list<T> list)
        : data(list) {
        this->create(t);
    }

    template <typename Tag>
    FlatSet(Tag t, ArrayView<const T> list) {
        data.pushAll(list.begin(), list.end());
        this->create(t);
    }

    template <typename Tag>
    FlatSet(Tag t, Array<T>&& values)
        : data(std::move(values)) {
        this->create(t);
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
    bool insert(U&& value) {
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
                return false;
            }
        }
        data.insert(from, std::forward<U>(value));
        return true;
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
        if (iter != data.end() && equal(*iter, value)) {
            return iter;
        } else {
            return data.end();
        }
    }

    Iterator<const T> find(const T& value) const {
        return const_cast<FlatSet*>(this)->find(value);
    }

    bool contains(const T& value) const {
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
    void create(ElementsSortedUniqueTag) {
        SPH_ASSERT(this->elementsSortedAndUnique());
    }

    void create(ElementsUniqueTag) {
        std::sort(data.begin(), data.end(), static_cast<const TLess&>(*this));
        SPH_ASSERT(this->elementsSortedAndUnique());
    }

    void create(ElementsCommonTag) {
        std::sort(data.begin(), data.end(), static_cast<const TLess&>(*this));
        auto end = std::unique(data.begin(), data.end(), [this](const T& t1, const T& t2) { //
            return this->equal(t1, t2);
        });
        data.resize(end - data.begin());
        SPH_ASSERT(this->elementsSortedAndUnique());
    }

    bool elementsSortedAndUnique() const {
        if (!std::is_sorted(data.begin(), data.end(), static_cast<const TLess&>(*this))) {
            return false;
        }

        for (Size i = 1; i < data.size(); ++i) {
            if (equal(data[i], data[i - 1])) {
                return false;
            }
        }
        return true;
    }

    INLINE bool less(const T& t1, const T& t2) const {
        return TLess::operator()(t1, t2);
    }

    INLINE bool equal(const T& t1, const T& t2) const {
        return !less(t1, t2) && !less(t2, t1);
    }
};

NAMESPACE_SPH_END
