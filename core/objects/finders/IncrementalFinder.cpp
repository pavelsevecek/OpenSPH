#include "objects/finders/IncrementalFinder.h"

NAMESPACE_SPH_BEGIN

IncrementalFinder::Handle IncrementalFinder::addPoint(const Vector& p) {
    const Indices idxs = floor(p / cellSize);
    Cell& cell = map[idxs];
    cell.push(p);
    count++;
    return Handle(idxs, cell.size() - 1, {});
}

void IncrementalFinder::addPoints(ArrayView<const Vector> points) {
    for (const Vector& p : points) {
        this->addPoint(p);
    }
}

Vector IncrementalFinder::point(const Handle& handle) const {
    return map.at(handle.coords())[handle.index()];
}

Array<Vector> IncrementalFinder::array() const {
    Array<Vector> result;
    for (const auto& cell : map) {
        for (const Vector& p : cell.second) {
            result.push(p);
        }
    }
    return result;
}

Size IncrementalFinder::size() const {
    return count;
}

Size IncrementalFinder::getNeighCnt(const Vector& center, const Float radius) const {
    Size count = 0;
    this->findAll(center, radius, [&count](const Handle& UNUSED(handle)) { //
        ++count;
    });
    return count;
}

void IncrementalFinder::findAll(const Vector& center, const Float radius, Array<Handle>& handles) const {
    handles.clear();
    this->findAll(center, radius, [&handles](const Handle& handle) { //
        handles.push(handle);
    });
}

void IncrementalFinder::findAll(const Vector& center, const Float radius, Array<Vector>& neighs) const {
    neighs.clear();
    this->findAll(center, radius, [this, &neighs](const Handle& handle) { //
        neighs.push(point(handle));
    });
}


template <typename TAdd>
void IncrementalFinder::findAll(const Vector& center, const Float radius, const TAdd& add) const {
    const Sphere search(center, radius);
    const Indices idxs0 = floor(center / cellSize);

    Indices left = idxs0;
    Indices right = idxs0;
    for (Size i = 0; i < 3; ++i) {
        Indices next = idxs0;
        while (true) {
            next[i]++;
            if (search.overlaps(cellBox(next))) {
                right[i] = next[i];
            } else {
                break;
            }
        }

        next = idxs0;
        while (true) {
            next[i]--;
            if (search.overlaps(cellBox(next))) {
                left[i] = next[i];
            } else {
                break;
            }
        }
    }

    for (int z = left[2]; z <= right[2]; ++z) {
        for (int y = left[1]; y <= right[1]; ++y) {
            for (int x = left[0]; x <= right[0]; ++x) {
                const Indices idxs(x, y, z);
                const auto iter = map.find(idxs);
                if (iter == map.end()) {
                    continue;
                }

                for (Size i = 0; i < iter->second.size(); ++i) {
                    const Vector& p = iter->second[i];
                    const Float distSqr = getSqrLength(p - center);
                    if (distSqr < sqr(radius)) {
                        add(Handle(idxs, i, {}));
                    }
                }
            }
        }
    }
}

NAMESPACE_SPH_END
