#pragma once

/// \file FlatSet.h
/// \brief Container storing sorted unique values.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

template <typename T>
class FlatSet {
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

    void insert(const T& value) {
        Size from = 0;
        Size to = data.size();
        Size mid = Size(-1);

        while (from < to && from != mid) {
            mid = (from + to) / 2;
            if (data[mid] < value) {
                from = mid + 1;
            } else if (value < data[mid]) {
                to = mid;
            } else {
                // found the same value, skip
                return;
            }
        }
        data.insert(from, value);
    }

    Iterator<T> find(const T& value) {
        Size from = 0;
        Size to = data.size();
        Size mid = Size(-1);

        while (from < to && from != mid) {
            mid = (from + to) / 2;
            if (data[mid] < value) {
                from = mid + 1;
            } else if (data[mid] == value) {
                return this->begin() + mid;
            } else {
                to = mid;
            }
        }
        return this->end();
    }

    Iterator<const T> find(const T& value) const {
        return const_cast<FlatSet*>(this)->find(value);
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
};

NAMESPACE_SPH_END
