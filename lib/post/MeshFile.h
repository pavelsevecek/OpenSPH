#pragma once

#include "io/Path.h"
#include "objects/finders/Order.h"
#include "objects/wrappers/Outcome.h"
#include "post/MarchingCubes.h" /// \todo move Triangle elsewhere so that we don't have to include whole MC

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class MeshFile : public Polymorphic {
    public:
        virtual Outcome save(const Path& path, ArrayView<const Triangle> triangles) = 0;
    };
}

class PlyFile : public Abstract::MeshFile {
public:
    virtual Outcome save(const Path& path, ArrayView<const Triangle> triangles) override;

protected:
    void getVerticesAndIndices(ArrayView<const Triangle> triangles,
        Array<Vector>& vertices,
        Array<Size>& indices,
        const Float eps);
};

NAMESPACE_SPH_END
