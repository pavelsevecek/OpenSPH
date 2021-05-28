#pragma once

#include "objects/geometry/Triangle.h"

NAMESPACE_SPH_BEGIN

struct Mesh {
    Array<Vector> vertices;

    class Face {
    private:
        StaticArray<Size, 3> idxs;

    public:
        Face() = default;

        Face(const Face& other)
            : idxs{ other[0], other[1], other[2] } {}

        Face(const Size a, const Size b, const Size c)
            : idxs{ a, b, c } {}

        Face& operator=(const Face& other) {
            idxs[0] = other[0];
            idxs[1] = other[1];
            idxs[2] = other[2];
            return *this;
        }

        Size& operator[](const int i) {
            return idxs[i];
        }

        Size operator[](const int i) const {
            return idxs[i];
        }

        bool operator==(const Face& other) const {
            return idxs == other.idxs;
        }

        bool operator!=(const Face& other) const {
            return idxs != other.idxs;
        }
    };

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
