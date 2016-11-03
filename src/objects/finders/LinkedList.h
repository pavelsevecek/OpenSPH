#pragma once

#include "geometry/Bounds.h"
#include "objects/finders/Finder.h"
#include "objects/wrappers/Iterators.h"
#include <algorithm>
#include <iostream>

NAMESPACE_SPH_BEGIN

/*
class LookupMap : public Noncopyable {
private:
    Array<int> storage;
    int dimensionSize;


    int map(const Vector& v) const {
            return v[X] * Math::sqr(dimensionSize) + v[Y] * dimensionSize + v[Z];
    }

public:
    LookupMap() = default;

    LookupMap(const int n)
        : storage(Math::pow<3>(n))
        , dimensionSize(n) {
        // clear all cells
        storage.fill(0);
    }

    LookupMap& operator=(LookupMap&& other) {
        storage       = std::move(other.storage);
        dimensionSize = other.dimensionSize;
        return *this;
    }

    int& operator()(const StaticArray<int, 3>& v) {
        const int idx = map(v);
        ASSERT(unsigned(idx) < unsigned(storage.getSize()));
        return storage[idx];
    }

    int operator()(const Vector<int, d>& v) const {
        const int idx = map(v);
        ASSERT(unsigned(idx) < unsigned(storage.getSize()));
        return storage[idx];
    }
};

template <typename T = Evt::T, int d = Evt::d>
class LinkedList : public Abstract::Finder<T, d> {
private:
    /// \todo specialize vector for ints
    VectorOrder<d> sortedIndices;
    VectorOrder<d> rank;

    LookupMap<T, d> map;
    int cellCnt;
    Field<T, d> lowerBounds;
    Field<T, d> upperBounds;
    Array<int> linkedList;


public:
    LinkedList(FieldView<T, d> values)
        : Abstract::Finder<T, d>(values)
        , sortedIndices(values.getSize())
        , rank(values.getSize())
        , linkedList(values.getSize()) {
        cellCnt = Math::root<d>(values.getSize()) + 1;

        lowerBounds.reallocate(cellCnt);
        upperBounds.reallocate(cellCnt);
        rebuild();
    }

    virtual int findNeighbours(const Vector<T, d>& point,
                               const T radius,
                               Neighbours<T>& neighbours,
                               Flags<FinderFlags> UNUSED(flags) = EMPTY_FLAGS,
                               const Base<T> UNUSED(error)      = 0.f) const override {

        return 0;
    }

    virtual int findNeighbours(const int index,
                               const T radius,
                               Neighbours<T>& neighbours,
                               Flags<FinderFlags> flags    = EMPTY_FLAGS,
                               const Base<T> UNUSED(error) = 0.f) const override {
        neighbours.clear();
        using U = Base<T>;
        const U cellCntSqrInv = U(1.) / Math::pow<d - 1>(cellCnt);
        const Vector<int, d> multiIdx = static_cast<Vector<int, d>>(rank[index] * cellCntSqrInv);
        const Box<T, d> bounds(this->values[index] - Vector<T, d>(radius),
                               this->values[index] - Vector<T, d>(radius));
        Box<int, d> boundIdx(multiIdx, multiIdx);
        for (int i = 0; i < d; ++i) {
            Vector<int, d>& lbi = boundIdx.lower();
            while (lbi[i] > 0 && bounds.lower()[i] <= lowerBounds[lbi[i]][i]) {
                lbi[i]--;
            }
        }
        Vector<int, d> maxIdx = multiIdx;
        for (int i = 0; i < d; ++i) {
            Vector<int, d>& ubi = boundIdx.upper();
            while (ubi[i] < upperBounds.getSize()-1 && bounds.upper()[i] <= upperBounds[ubi[i]][i]) {
                ubi[i]++;
            }
        }
        /// \todo
        boundIdx.extend(Vector<int, d>(0));
        boundIdx.extend(Vector<int, d>(upperBounds.getSize()-1));
        const int refRankH =
            flags.has(FinderFlags::FIND_ONLY_SMALLER_H) ? rank[index][H] : this->values.getSize();
        boundIdx.iterate(Vector<int, d>(1), [this, &neighbours, refRankH, radius, index](Vector<int, d>&&
idxs) {
            int cell = map(std::move(idxs));
            while (cell != 0) {
                auto lengthSqr = getSqrLength(this->values[cell] - this->values[index]);
                if (rank[cell][H] < refRankH && lengthSqr < Math::sqr(radius)) {
                    neighbours.push(NeighbourRecord<T>{ cell, lengthSqr} );
                }
                cell = linkedList[cell];
            }
        });


        return 0;
    }


    virtual void rebuild() override {
        for (int i = 0; i < d; ++i) {
            sortedIndices.shuffle(i, [this, i](int idx1, int idx2) {
                return this->values[idx1][i] < this->values[idx2][i];
            });
        }
        /// extra dimension - sort smoothing length
        sortedIndices.shuffle(H, [this](int idx1, int idx2) {
            return this->values[idx1][H] < this->values[idx2][H];
        });
        using U = Base<T>;
        rank    = sortedIndices.getInverted();
        map     = LookupMap<T, d>(cellCnt);
        lowerBounds.fill(Vector<T, d>(INFTY));
        upperBounds.fill(Vector<T, d>(-INFTY));
        const U cellCntSqrInv = U(1.) / Math::pow<d - 1>(cellCnt);

        for (int idx = 0; idx < this->values.getSize(); ++idx) {
            const Vector<int, d> multiIdx = static_cast<Vector<int, d>>(rank[idx] * cellCntSqrInv);
            int& cell       = map(multiIdx);
            linkedList[idx] = cell;
            cell            = idx;
            /// \todo optimize using multiindices
            for (int i = 0; i < d; ++i) {
                T& lb = lowerBounds[multiIdx[i]][i];
                lb    = Math::min(lb, this->values[idx][i]);
                T& ub = upperBounds[multiIdx[i]][i];
                ub    = Math::max(ub, this->values[idx][i]);
            }
        }
        std::cout << "Linked list: " << std::endl;
        for (int i=0; i<linkedList.getSize(); ++i) {
            std::cout << linkedList[i] << std::endl;
        }
    }
};
*/
NAMESPACE_SPH_END
