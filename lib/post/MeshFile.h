#pragma once

#include "io/Path.h"
#include "objects/finders/Order.h"
#include "objects/geometry/Triangle.h"
#include "objects/wrappers/Expected.h"
#include "objects/wrappers/Outcome.h"

NAMESPACE_SPH_BEGIN

/// \brief Converts array of triangles into array of vertices (without duplicate vertices) and array of
/// indices into the vertex array, where each three consecutive indices define a triangle.
///
/// \param triangles Input array of triangles.
/// \param vertices Output array of vertices. All previous elements are cleared.
/// \param indices Output array of indices. All previous elements are cleared.
/// \param eps Tolerancy for two vertices to be considered equal.
///
/// \internal Currently exposed in header for testing purposes. Should be moved elsewhere if it is needed by
/// other component of the code.
void getVerticesAndIndices(ArrayView<const Triangle> triangles,
    Array<Vector>& vertices,
    Array<Size>& indices,
    const Float eps);

/// \brief Interface for mesh exporters.
class IMeshFile : public Polymorphic {
public:
    virtual Outcome save(const Path& path, ArrayView<const Triangle> triangles) = 0;

    virtual Expected<Array<Triangle>> load(const Path& path) = 0;
};

/// \brief Exports meshes into a Polygon File Format (.ply file).
///
/// See https://en.wikipedia.org/wiki/PLY_(file_format)
class PlyFile : public IMeshFile {
public:
    virtual Outcome save(const Path& path, ArrayView<const Triangle> triangles) override;

    virtual Expected<Array<Triangle>> load(const Path& path) override;
};

/// \brief Simple text format containing mesh vertictes and triangle indices.
///
/// Used for example by Gaskell Itokawa Shape Model (https://sbn.psi.edu/pds/resource/itokawashape.html).
class TabFile : public IMeshFile {
private:
    Float lengthUnit;

public:
    /// \param lengthUnit Conversion factor from the loaded values to meters.
    explicit TabFile(const Float lengthUnit = 1.e3_f);

    virtual Outcome save(const Path& path, ArrayView<const Triangle> triangles) override;

    virtual Expected<Array<Triangle>> load(const Path& path) override;
};

NAMESPACE_SPH_END
