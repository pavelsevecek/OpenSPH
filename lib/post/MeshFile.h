#pragma once

#include "io/Path.h"
#include "objects/finders/Order.h"
#include "objects/geometry/Triangle.h"
#include "objects/wrappers/Expected.h"
#include "objects/wrappers/Outcome.h"

NAMESPACE_SPH_BEGIN

/// \brief Interface for mesh exporters.
class IMeshFile : public Polymorphic {
public:
    virtual Outcome save(const Path& path, ArrayView<const Triangle> triangles) = 0;

    virtual Expected<Array<Triangle>> load(const Path& path) = 0;
};

/// \brief Exported of meshes into a ply format.
class PlyFile : public IMeshFile {
public:
    virtual Outcome save(const Path& path, ArrayView<const Triangle> triangles) override;

    virtual Expected<Array<Triangle>> load(const Path& path) override;

protected:
    void getVerticesAndIndices(ArrayView<const Triangle> triangles,
        Array<Vector>& vertices,
        Array<Size>& indices,
        const Float eps);
};

NAMESPACE_SPH_END
