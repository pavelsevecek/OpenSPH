#include "post/Mesh.h"
#include "objects/finders/Order.h"
#include <map>
#include <set>

NAMESPACE_SPH_BEGIN

inline Tuple<int, int> makeEdge(const int i1, const int i2) {
    return makeTuple(min(i1, i2), max(i1, i2));
}

bool isMeshClosed(const Mesh& mesh) {
    std::map<Tuple<int, int>, Array<Size>> edges;
    for (Size i = 0; i < mesh.faces.size(); ++i) {
        const Mesh::Face& f = mesh.faces[i];
        edges[makeEdge(f[0], f[1])].push(i);
        edges[makeEdge(f[0], f[2])].push(i);
        edges[makeEdge(f[1], f[2])].push(i);
    }

    for (auto& p : edges) {
        if (p.second.size() != 2) {
            return false; /// \todo or invalid (edge shared by >2 faces)
        }
    }
    return true;
}

void refineMesh(Mesh& mesh) {
    Array<std::set<Size>> vertexNeighs(mesh.vertices.size());
    for (Size i = 0; i < mesh.faces.size(); ++i) {
        const Mesh::Face& f = mesh.faces[i];
        vertexNeighs[f[0]].insert(f[1]);
        vertexNeighs[f[0]].insert(f[2]);
        vertexNeighs[f[1]].insert(f[0]);
        vertexNeighs[f[1]].insert(f[2]);
        vertexNeighs[f[2]].insert(f[0]);
        vertexNeighs[f[2]].insert(f[1]);
    }

    Array<Vector> grads(mesh.vertices.size());
    for (Size i = 0; i < mesh.vertices.size(); ++i) {
        Vector grad = -mesh.vertices[i];

        for (Size j : vertexNeighs[i]) {
            grad += mesh.vertices[j] / Float(vertexNeighs[i].size());
        }
        grads[i] = grad;
    }

    for (Size i = 0; i < mesh.vertices.size(); ++i) {
        mesh.vertices[i] += 0.5f * grads[i];
    }
}

void subdivideMesh(Mesh& mesh) {
    Array<Mesh::Face> newFaces;
    for (Mesh::Face& face : mesh.faces) {
        const Vector p1 = mesh.vertices[face[0]];
        const Vector p2 = mesh.vertices[face[1]];
        const Vector p3 = mesh.vertices[face[2]];
        const Vector p12 = 0.5_f * (p1 + p2);
        const Vector p13 = 0.5_f * (p1 + p3);
        const Vector p23 = 0.5_f * (p2 + p3);
        const Size i2 = face[1];
        const Size i3 = face[2];
        const Size i12 = mesh.vertices.size();
        const Size i13 = i12 + 1;
        const Size i23 = i12 + 2;
        mesh.vertices.push(p12);
        mesh.vertices.push(p13);
        mesh.vertices.push(p23);

        newFaces.push(Mesh::Face{ i12, i2, i23 });
        newFaces.push(Mesh::Face{ i13, i23, i3 });
        newFaces.push(Mesh::Face{ i13, i12, i23 });
        face[1] = i12;
        face[2] = i13;
    }
    mesh.faces.pushAll(std::move(newFaces));
}

Mesh getMeshFromTriangles(ArrayView<const Triangle> triangles, const Float eps) {
    Mesh mesh;
    mesh.faces.resize(triangles.size());

    // get order of vertices sorted lexicographically
    Order lexicographicalOrder(triangles.size() * 3);
    lexicographicalOrder.shuffle([&triangles](const Size i1, const Size i2) {
        const Vector v1 = triangles[i1 / 3][i1 % 3];
        const Vector v2 = triangles[i2 / 3][i2 % 3];
        return lexicographicalLess(v1, v2);
    });

    // get inverted permutation: maps index of particle to its position in lexicographically sorted array
    Order mappingOrder = lexicographicalOrder.getInverted();

    // holds indices of vertices already used, maps index in input array to index in output array
    Array<Size> insertedVerticesIdxs(triangles.size() * 3);
    insertedVerticesIdxs.fill(Size(-1));

    for (Size i = 0; i < triangles.size(); ++i) {
        const Triangle& tri = triangles[i];
        for (Size j = 0; j < 3; ++j) {
            // get order in sorted array
            const Size idxInSorted = mappingOrder[3 * i + j];

            bool vertexFound = false;

            auto check = [&](const Size k) {
                const Size idxInInput = lexicographicalOrder[k];
                const Vector v = triangles[idxInInput / 3][idxInInput % 3];
                if (almostEqual(tri[j], v, eps)) {
                    if (insertedVerticesIdxs[idxInInput] != Size(-1)) {
                        // this vertex was already found; just push the index
                        mesh.faces[i][j] = insertedVerticesIdxs[idxInInput];
                        // also update the index of this verted (not really needed, might speed things up)
                        insertedVerticesIdxs[3 * i + j] = insertedVerticesIdxs[idxInInput];
                        vertexFound = true;
                        return false;
                    } else {
                        // same vertex, but also not used, keep going
                        return true;
                    }
                } else {
                    // different vertex
                    return false;
                }
            };
            // look for equal (or almost equal) vertices higher in order and check whether we used them
            for (Size k = idxInSorted + 1; k < lexicographicalOrder.size(); ++k) {
                if (!check(k)) {
                    break;
                }
            }
            // also look for vertices lower in order (use signed k!)
            if (!vertexFound) {
                for (int k = idxInSorted - 1; k >= 0; --k) {
                    if (!check(k)) {
                        break;
                    }
                }
            }

            if (!vertexFound) {
                // update the map
                insertedVerticesIdxs[3 * i + j] = mesh.vertices.size();

                // add the current vertex and its index
                mesh.faces[i][j] = mesh.vertices.size();
                mesh.vertices.push(tri[j]);
            }
        }
    }

    // remove degenerate faces
    Array<Size> toRemove;
    for (Size i = 0; i < mesh.faces.size(); ++i) {
        const Mesh::Face& f = mesh.faces[i];
        if (f[0] == f[1] || f[1] == f[2] || f[0] == f[2]) {
            toRemove.push(i);
        }
    }
    mesh.faces.remove(toRemove);

    return mesh;
}

Array<Triangle> getTrianglesFromMesh(const Mesh& mesh) {
    Array<Triangle> triangles(mesh.faces.size());
    for (Size i = 0; i < mesh.faces.size(); ++i) {
        triangles[i] = Triangle(mesh.vertices[mesh.faces[i][0]],
            mesh.vertices[mesh.faces[i][1]],
            mesh.vertices[mesh.faces[i][2]]);
    }
    return triangles;
}

NAMESPACE_SPH_END
