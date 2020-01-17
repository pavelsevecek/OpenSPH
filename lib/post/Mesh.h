#pragma once

#include "objects/geometry/Triangle.h"

NAMESPACE_SPH_BEGIN

struct Mesh {
    Array<Vector> vertices;

    using Face = std::array<Size, 3>;

    Array<Face> faces;
};

/// \brief Checks if the mesh represents a single closed surface.
bool isMeshClosed(const Mesh& mesh);

/// \brief Improves mesh quality using edge flips (valence equalization) and tangential relaxation.
void refineMesh(Mesh& mesh);

/// \brief Subdivides all triangles of the mesh using 1-4 scheme
void subdivideMesh(Mesh& mesh);

/// \brief Converts array of triangles into a mesh.
///
/// The mesh contains a set of unique vertices and faces as indices into the vertex array.
/// \param triangles Input array of triangles.
/// \param eps Tolerancy for two vertices to be considered equal.
Mesh getMeshFromTriangles(ArrayView<const Triangle> triangles, const Float eps);

/// \brief Expands the mesh into an array of triangles
Array<Triangle> getTrianglesFromMesh(const Mesh& mesh);

NAMESPACE_SPH_END
