#include "post/MeshFile.h"
#include "io/Logger.h"
#include <fstream>

NAMESPACE_SPH_BEGIN

void getVerticesAndIndices(ArrayView<const Triangle> triangles,
    Array<Vector>& vertices,
    Array<Size>& indices,
    const Float eps) {
    vertices.clear();
    indices.clear();

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
        getVerticesAndIndices(triangles, vtxs, idxs, 0._f);
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

static Optional<std::string> stripStart(const std::string& line, const std::string& format) {
    if (line.size() < format.size()) {
        return NOTHING;
    }
    if (line.substr(0, format.size()) == format) {
        // remove the following space, if there is one
        return line.substr(min(line.size(), format.size() + 1));
    } else {
        return NOTHING;
    }
}

Expected<Array<Triangle>> PlyFile::load(const Path& path) {
    try {
        std::ifstream ifs(path.native());
        std::string line;

        // check for the file format
        if (!std::getline(ifs, line) || line != "ply") {
            throw IoError("File does not have a valid .ply format");
        }
        if (!std::getline(ifs, line) || line != "format ascii 1.0") {
            throw IoError("Only ascii format of the .ply file is currently supported");
        }

        // parse the header
        Size vertexCnt = Size(-1);
        Size faceCnt = Size(-1);
        Array<std::string> properties;
        while (std::getline(ifs, line)) {
            if (stripStart(line, "comment")) {
                continue;
            } else if (stripStart(line, "end_header")) {
                break;
            } else if (Optional<std::string> vertexStr = stripStart(line, "element vertex")) {
                vertexCnt = std::stoul(vertexStr.value());
            } else if (Optional<std::string> faceStr = stripStart(line, "element face")) {
                faceCnt = std::stoul(faceStr.value());
            } else if (Optional<std::string> property = stripStart(line, "property float")) {
                properties.push(property.value());
            }
        }

        // check validity of the header info
        if (faceCnt == Size(-1) || vertexCnt == Size(-1)) {
            throw IoError("Header did not contain number of faces or vertices");
        } else if (properties.size() < 3 || properties[0] != "x" || properties[1] != "y" ||
                   properties[2] != "z") {
            throw IoError(
                "Currently, only files where x, y, z are the first 3 float properties are supported");
        }

        // parse the vertex data
        Array<Vector> vertices;
        while (vertices.size() < vertexCnt) {
            std::getline(ifs, line);
            if (ifs.fail()) {
                break;
            }
            Vector v;
            std::stringstream ss(line);
            /// \todo can be read directly from ifs, no need to go through stringstream
            ss >> v[X] >> v[Y] >> v[Z];
            // skip the other properties
            for (Size i = 0; i < properties.size() - 3; ++i) {
                Float dummy;
                ss >> dummy;
            }
            if (!ss) {
                throw IoError("Invalid line format when reading the vertex data");
            }
            vertices.push(v);
        }
        if (vertices.size() != vertexCnt) {
            throw IoError("Incorrect number of vertices in the file");
        }

        // parse the faces and generate the list of triangles
        Array<Triangle> triangles;
        while (triangles.size() < faceCnt) {
            std::getline(ifs, line);
            if (ifs.fail()) {
                break;
            }
            Size cnt, i, j, k;
            std::stringstream ss(line);
            ss >> cnt >> i >> j >> k;
            if (!ss || cnt != 3) {
                throw IoError("Invalid line format when reading the index data");
            }
            triangles.emplaceBack(vertices[i], vertices[j], vertices[k]);
        }
        return std::move(triangles);

    } catch (std::exception& e) {
        return makeUnexpected<Array<Triangle>>(e.what());
    }
}

TabFile::TabFile(const Float lengthUnit)
    : lengthUnit(lengthUnit) {}

Outcome TabFile::save(const Path& path, ArrayView<const Triangle> triangles) {
    (void)path;
    (void)triangles;
    NOT_IMPLEMENTED;
}

Expected<Array<Triangle>> TabFile::load(const Path& path) {
    try {
        std::ifstream ifs(path.native());
        Size vertexCnt, triangleCnt;
        ifs >> vertexCnt >> triangleCnt;
        if (!ifs) {
            throw IoError("Invalid format: cannot read file header");
        }

        Array<Vector> vertices;
        Vector v;
        Size dummy;
        for (Size vertexIdx = 0; vertexIdx < vertexCnt; ++vertexIdx) {
            ifs >> dummy >> v[X] >> v[Y] >> v[Z];
            v *= lengthUnit;
            vertices.push(v);
        }

        Array<Triangle> triangles;
        Size i, j, k;
        for (Size triangleIdx = 0; triangleIdx < triangleCnt; ++triangleIdx) {
            ifs >> dummy >> i >> j >> k;
            // indices start at 1 in .tab
            triangles.emplaceBack(vertices[i - 1], vertices[j - 1], vertices[k - 1]);
        }
        return std::move(triangles);

    } catch (std::exception& e) {
        return makeUnexpected<Array<Triangle>>(e.what());
    }
}

Outcome ObjFile::save(const Path& path, ArrayView<const Triangle> triangles) {
    (void)path;
    (void)triangles;
    NOT_IMPLEMENTED;
}

Expected<Array<Triangle>> ObjFile::load(const Path& path) {
    try {
        std::ifstream ifs(path.native());
        Array<Vector> vertices;
        Array<Triangle> triangles;

        char symb;
        while (ifs) {
            ifs >> symb;
            if (symb == 'v') {
                Vector v;
                ifs >> v[X] >> v[Y] >> v[Z];
                vertices.push(v);
            } else if (symb == 'f') {
                Size i, j, k;
                ifs >> i >> j >> k;
                triangles.emplaceBack(vertices[i - 1], vertices[j - 1], vertices[k - 1]);
            }
        }

        return std::move(triangles);

    } catch (std::exception& e) {
        return makeUnexpected<Array<Triangle>>(e.what());
    }
}

NAMESPACE_SPH_END
