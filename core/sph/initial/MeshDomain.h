#pragma once

/// \file MeshDomain.h
/// \brief Domain represented by triangular mesh.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "objects/containers/Volume.h"
#include "objects/finders/Bvh.h"
#include "objects/geometry/Domain.h"
#include "objects/geometry/Triangle.h"

NAMESPACE_SPH_BEGIN

class IScheduler;

struct MeshParams {
    /// Arbitrary transformation matrix applied on the mesh
    AffineMatrix matrix = AffineMatrix::identity();

    /// If true, cached volume is created to allow fast calls of \ref contains
    bool precomputeInside = true;

    /// Resolution of the volume, used if precomputeInside == true
    Size volumeResolution = 128;
};


/// \brief Domain represented by triangular mesh.
class MeshDomain : public IDomain {
private:
    Bvh<BvhTriangle> bvh;
    Volume<char> mask;

    /// Cached values, so that we do not have to keep a separate list of triangles
    struct {
        Array<Vector> points;
        Array<Vector> normals;
        Box box;
        Float volume;
        Float area;
    } cached;

public:
    MeshDomain(IScheduler& scheduler, Array<Triangle>&& triangles, const MeshParams& params = MeshParams{});

    virtual Vector getCenter() const override {
        return cached.box.center();
    }

    virtual Box getBoundingBox() const override {
        return cached.box;
    }

    virtual Float getVolume() const override {
        return cached.volume;
    }

    virtual Float getSurfaceArea() const override {
        return cached.area;
    }

    virtual bool contains(const Vector& v) const override;

    virtual void getSubset(ArrayView<const Vector> vs,
        Array<Size>& output,
        const SubsetType type) const override;

    virtual void getDistanceToBoundary(ArrayView<const Vector>, Array<Float>&) const override {
        NOT_IMPLEMENTED;
    }

    virtual void project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices) const override;

    virtual void addGhosts(ArrayView<const Vector> vs,
        Array<Ghost>& ghosts,
        const Float eta,
        const Float eps) const override;

private:
    bool containImpl(const Vector& v) const;
};

NAMESPACE_SPH_END
