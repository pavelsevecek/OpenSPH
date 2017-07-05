#pragma once

#include "geometry/Box.h"
#include "geometry/Multipole.h"
#include "objects/finders/AbstractFinder.h"

NAMESPACE_SPH_BEGIN


struct KdNode {
    /// Here X, Y, Z must be 0, 1, 2
    enum class Type { X, Y, Z, LEAF };
    Type type;

    Multipole<4> M;

    KdNode(const Type& type)
        : type(type) {}

    INLINE bool isLeaf() const {
        return type == Type::LEAF;
    }
};


/// https://www.cs.umd.edu/~mount/Papers/cgc99-smpack.pdf
class KdTree : public Abstract::Finder {
private:
    Size leafSize;
    Box entireBox;

    Array<Size> idxs;

    /// \todo possibly make KdTree templated to allow modifying Node

    struct PlaceHolderNode : public KdNode {
        Size padding[3];

        PlaceHolderNode(const KdNode::Type& type)
            : KdNode(type) {}
    };

    Array<PlaceHolderNode> nodes;

    Array<Box> leafBoxes;

    static constexpr Size ROOT_NODE = -1;

public:
    KdTree(const Size leafSize = 20)
        : leafSize(leafSize) {}

    virtual Size findNeighbours(const Size index,
        const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlags> flags = EMPTY_FLAGS,
        const Float error = 0._f) const override;

    /// Finds all points within given radius from given position. The position may not correspond to any
    /// point.
    virtual Size findNeighbours(const Vector& position,
        const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlags> flags = EMPTY_FLAGS,
        const Float error = 0._f) const override {
        MARK_USED(position);
        MARK_USED(radius);
        MARK_USED(neighbours);
        MARK_USED(flags);
        MARK_USED(error);
        return 0;
    }

    bool sanityCheck() const;

protected:
    virtual void buildImpl(ArrayView<const Vector> points) override;

    virtual void rebuildImpl(ArrayView<const Vector> points) override;

private:
    void init();

    void buildTree(const Size parent, const Size from, const Size to, const Box& box, const Size slidingCnt);

    void addLeaf(const Size parent, const Size from, const Size to);

    void addInner(const Size parent, const Float splitPosition, const Size splitIdx);

    bool isSingular(const Size from, const Size to, const Size splitIdx) const;

    bool checkBoxes(const Size from, const Size to, const Size mid, const Box& box1, const Box& box2) const;
};

NAMESPACE_SPH_END
