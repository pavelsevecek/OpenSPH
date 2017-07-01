#include "post/MeshFile.h"
#include <fstream>

NAMESPACE_SPH_BEGIN

void PlyFile::getVerticesAndIndices(ArrayView<const Triangle> triangles,
    Array<Vector>& vertices,
    Array<Size>& indices,
    const Float eps) {
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
                        indices.push(insertedVerticesIdxs[idxInInput]);
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
                insertedVerticesIdxs[3 * i + j] = vertices.size();

                // add the current vertex and its index
                indices.push(vertices.size());
                vertices.push(tri[j]);
            }
        }
    }
}

Outcome PlyFile::save(const Path& path, ArrayView<const Triangle> triangles) {
    try {
        std::ofstream ofs(path.native());
        // http://paulbourke.net/dataformats/ply/
        ofs << "ply\n";
        ofs << "format ascii 1.0\n";
        ofs << "comment Exported by " << CODE_NAME << "\n";

        Array<Vector> vtxs;
        Array<Size> idxs;
        this->getVerticesAndIndices(triangles, vtxs, idxs, 0._f);
        ASSERT(vtxs.size() <= idxs.size() && (idxs.size() % 3 == 0)); // sanity check

        ofs << "element vertex " << vtxs.size() << "\n";
        ofs << "property float x\n";
        ofs << "property float y\n";
        ofs << "property float z\n";
        ofs << "element face " << idxs.size() / 3 << "\n";
        ofs << "property list int int vertex_index\n";
        ofs << "end_header\n";

        for (Vector& v : vtxs) {
            // print components manually, we don't need formatted result here
            ofs << v[X] << " " << v[Y] << " " << v[Z] << std::endl;
        }
        for (Size i = 0; i < idxs.size() / 3; ++i) {
            ofs << "3 " << idxs[3 * i] << " " << idxs[3 * i + 1] << " " << idxs[3 * i + 2] << std::endl;
        }
        return SUCCESS;
    } catch (std::exception& e) {
        return e.what();
    }
}

NAMESPACE_SPH_END
