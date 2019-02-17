#include "objects/finders/Bvh.h"

NAMESPACE_SPH_BEGIN

struct BvhTraversal {
    Size idx;
    Float t_min;
};

struct BvhBuildEntry {
    Size parent;
    Size start;
    Size end;
};

bool intersectBox(const Box& box, const Ray& ray, Float& t_min, Float& t_max) {
    StaticArray<Vector, 2> b = { box.lower(), box.upper() };
    Float tmin = (b[ray.signs[X]][X] - ray.orig[X]) * ray.invDir[X];
    Float tmax = (b[1 - ray.signs[X]][X] - ray.orig[X]) * ray.invDir[X];
    ASSERT(!std::isnan(tmin) && !std::isnan(tmax)); // they may be inf though
    const Float tymin = (b[ray.signs[Y]][Y] - ray.orig[Y]) * ray.invDir[Y];
    const Float tymax = (b[1 - ray.signs[Y]][Y] - ray.orig[Y]) * ray.invDir[Y];
    ASSERT(!std::isnan(tymin) && !std::isnan(tymax));

    if ((tmin > tymax) || (tymin > tmax)) {
        return false;
    }
    tmin = max(tmin, tymin);
    tmax = min(tmax, tymax);

    const Float tzmin = (b[ray.signs[Z]][Z] - ray.orig[Z]) * ray.invDir[Z];
    const Float tzmax = (b[1 - ray.signs[Z]][Z] - ray.orig[Z]) * ray.invDir[Z];
    ASSERT(!std::isnan(tzmin) && !std::isnan(tzmax));

    if ((tmin > tzmax) || (tzmin > tmax)) {
        return false;
    }
    tmin = max(tmin, tzmin);
    tmax = min(tmax, tzmax);

    t_min = tmin;
    t_max = tmax;

    return true;
}

template <typename TBvhObject>
template <typename TAddIntersection>
void Bvh<TBvhObject>::getIntersections(const Ray& ray, const TAddIntersection& addIntersection) const {
    StaticArray<Float, 4> boxHits;
    Size closer;
    Size other;

    StaticArray<BvhTraversal, 64> stack;
    int stackIdx = 0;

    stack[stackIdx].idx = 0;
    stack[stackIdx].t_min = 0._f; // -INFTY;

    while (stackIdx >= 0) {
        const Size idx = stack[stackIdx].idx;
        // const Float t_min = stack[stackIdx].t_min;
        stackIdx--;
        const BvhNode& node = nodes[idx];

        /// \todo optimization for the single intersection case
        /*        if (t_min > intersection.t) {
                    continue;
                }*/
        if (node.rightOffset == 0) {
            // leaf
            for (Size primIdx = 0; primIdx < node.primCnt; ++primIdx) {
                IntersectionInfo current;

                const TBvhObject& obj = objects[node.start + primIdx];
                const bool hit = obj.getIntersection(ray, current);

                if (hit) {
                    if (!addIntersection(current)) {
                        // bailout
                        return;
                    }
                }
            }
        } else {
            // inner node
            const bool hitc0 = intersectBox(nodes[idx + 1].box, ray, boxHits[0], boxHits[1]);
            const bool hitc1 = intersectBox(nodes[idx + node.rightOffset].box, ray, boxHits[2], boxHits[3]);

            if (hitc0 && hitc1) {
                closer = idx + 1;
                other = idx + node.rightOffset;

                if (boxHits[2] < boxHits[0]) {
                    std::swap(boxHits[0], boxHits[2]);
                    std::swap(boxHits[1], boxHits[3]);
                    std::swap(closer, other);
                }

                stack[++stackIdx] = BvhTraversal{ other, boxHits[2] };
                stack[++stackIdx] = BvhTraversal{ closer, boxHits[0] };
            } else if (hitc0) {
                stack[++stackIdx] = BvhTraversal{ idx + 1, boxHits[0] };
            } else if (hitc1) {
                stack[++stackIdx] = BvhTraversal{ idx + node.rightOffset, boxHits[2] };
            }
        }
    }
}

template <typename TBvhObject>
bool Bvh<TBvhObject>::getFirstIntersection(const Ray& ray, IntersectionInfo& intersection) const {
    intersection.t = INFTY;
    intersection.object = nullptr;

    this->getIntersections(ray, [&intersection](IntersectionInfo& current) {
        if (current.t < intersection.t) {
            intersection = current;
        }
        return true;
    });
    return intersection.object != nullptr;
}

template <typename TBvhObject>
void Bvh<TBvhObject>::getAllIntersections(const Ray& ray, std::set<IntersectionInfo>& intersections) const {
    intersections.clear();
    this->getIntersections(ray, [&intersections](IntersectionInfo& current) { //
        if (current.t > 0._f) {
            intersections.insert(current);
        }
        return true;
    });
}

template <typename TBvhObject>
bool Bvh<TBvhObject>::isOccluded(const Ray& ray) const {
    bool occluded = false;
    this->getIntersections(ray, [&occluded](IntersectionInfo&) {
        occluded = true;
        return false; // do not continue with traversal
    });
    return occluded;
}

template <typename TBvhObject>
void Bvh<TBvhObject>::build(Array<TBvhObject>&& objs) {
    objects = std::move(objs);
    ASSERT(!objects.empty());
    nodeCnt = 0;
    leafCnt = 0;

    StaticArray<BvhBuildEntry, 128> stack;
    Size stackIdx = 0;
    constexpr Size UNTOUCHED = 0xffffffff;
    constexpr Size NO_PARENT = 0xfffffffc;
    constexpr Size TOUCHED_TWICE = 0xfffffffd;

    // Push the root
    stack[stackIdx].start = 0;
    stack[stackIdx].end = objects.size();
    stack[stackIdx].parent = NO_PARENT;
    stackIdx++;

    BvhNode node;
    Array<BvhNode> buildNodes;
    buildNodes.reserve(2 * objects.size());

    while (stackIdx > 0) {
        BvhBuildEntry& nodeEntry = stack[--stackIdx];
        const Size start = nodeEntry.start;
        const Size end = nodeEntry.end;
        const Size primCnt = end - start;

        nodeCnt++;
        node.start = start;
        node.primCnt = primCnt;
        node.rightOffset = UNTOUCHED;

        Box bbox = objects[start].getBBox();
        const Vector center = objects[start].getCenter();
        Box boxCenter(center, center);
        for (Size i = start + 1; i < end; ++i) {
            bbox.extend(objects[i].getBBox());
            boxCenter.extend(objects[i].getCenter());
        }
        node.box = bbox;

        if (primCnt <= leafSize) {
            node.rightOffset = 0;
            leafCnt++;
        }
        buildNodes.push(node);

        if (nodeEntry.parent != NO_PARENT) {
            buildNodes[nodeEntry.parent].rightOffset--;

            if (buildNodes[nodeEntry.parent].rightOffset == TOUCHED_TWICE) {
                buildNodes[nodeEntry.parent].rightOffset = nodeCnt - 1 - nodeEntry.parent;
            }
        }

        if (node.rightOffset == 0) {
            continue;
        }

        const Size splitDim = argMax(boxCenter.size());
        const Float split = 0.5_f * (boxCenter.lower()[splitDim] + boxCenter.upper()[splitDim]);

        Size mid = start;
        for (Size i = start; i < end; ++i) {
            if (objects[i].getCenter()[splitDim] < split) {
                std::swap(objects[i], objects[mid]);
                ++mid;
            }
        }

        if (mid == start || mid == end) {
            mid = start + (end - start) / 2;
        }

        stack[stackIdx++] = { nodeCnt - 1, mid, end };
        stack[stackIdx++] = { nodeCnt - 1, start, mid };
    }

    ASSERT(buildNodes.size() == nodeCnt);
    nodes = std::move(buildNodes);
}

template <typename TBvhObject>
Box Bvh<TBvhObject>::getBoundingBox() const {
    return nodes[0].box;
}

template class Bvh<BvhTriangle>;
template class Bvh<BvhSphere>;
template class Bvh<BvhBox>;

NAMESPACE_SPH_END
