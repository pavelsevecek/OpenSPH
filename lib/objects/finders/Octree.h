#pragma once

/// Implementation of Octree algorithm for kNN queries.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

/// \todo UNFINISHED !!!

#include "objects/containers/Array.h"
#include "geometry/Vector.h"

template<typename T, int d>
struct Node {
    T* data;
    Node* children[1<<d];
};

template<typename T, int d>
class BoundingBox {
private:
    Vector<T, d> minBound, maxBound;

public:
    BoundingBox() :
        minBound(INFTY), maxBound(-INFTY) {
    }
    explicit BoundingBox(Array<Vector<T, d>& arr) {
        arr.forAll([](const Vector<T, d>& v){
            extend(v);
        });
    }
    void extend(const Vector<T, d>& v) {
        minBound = Vector<T, d>::min(minBound, v);
        maxBound = Vector<T, d>::max(maxBound, v);
    }
    Vector<T, d> getCenter() const {
        return 0.5f*(maxBound+minBound);
    }
    Vector<T, d> getDimensions() const {
        return Vector<T, d>::max(Vector<T, d>(0.f), maxBound-minBound);
    }
    T getVolume() const {
        T result = 1.f;
        Vector<T, d> dimensions = getDimensions();
        for (int i=0; i<d; ++i) {
            result *= dimensions[i];
        }
        return result;
    }
    bool isInside(const Vector<T, d>& v) {
        // todo: optimize
        for (int i=0; i<d; ++i) {
            if (v[i]<minBound[i] || v[i]>maxBound[i]) {
                return false;
            }
        }
        return true;
    }
};

template<typename T, int d>
class Octree {
private:
    BoundingBox<T, d> box;

public:
    Octree(const Array<Vector<T, d>>& vecs) {
        box.extend(vecs);
    }
};
