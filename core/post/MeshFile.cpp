#include "post/MeshFile.h"
#include "objects/Exceptions.h"
#include <fstream>

NAMESPACE_SPH_BEGIN

Outcome PlyFile::save(const Path& path, ArrayView<const Triangle> triangles) {
    try {
        std::ofstream ofs(path.native());
        // http://paulbourke.net/dataformats/ply/
        ofs << "ply\n";
        ofs << "format ascii 1.0\n";
        ofs << "comment Exported by " << SPH_CODE_NAME << "\n";

        Mesh mesh = getMeshFromTriangles(triangles, EPS);

        ofs << "element vertex " << mesh.vertices.size() << "\n";
        ofs << "property float x\n";
        ofs << "property float y\n";
        ofs << "property float z\n";
        ofs << "element face " << mesh.faces.size() << "\n";
        ofs << "property list int int vertex_index\n";
        ofs << "end_header\n";

        for (const Vector& v : mesh.vertices) {
            // print components manually, we don't need formatted result here
            ofs << v[X] << " " << v[Y] << " " << v[Z] << std::endl;
        }
        for (Size i = 0; i < mesh.faces.size(); ++i) {
            ofs << "3 " << mesh.faces[i][0] << " " << mesh.faces[i][1] << " " << mesh.faces[i][2]
                << std::endl;
        }
        return SUCCESS;
    } catch (const std::exception& e) {
        return makeFailed(exceptionMessage(e));
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
        return Expected<Array<Triangle>>(std::move(triangles));

    } catch (const std::exception& e) {
        return makeUnexpected<Array<Triangle>>(exceptionMessage(e));
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
        return Expected<Array<Triangle>>(std::move(triangles));

    } catch (const std::exception& e) {
        return makeUnexpected<Array<Triangle>>(exceptionMessage(e));
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

        std::string identifier;
        while (ifs) {
            ifs >> identifier;
            if (identifier == "v") {
                Vector v;
                ifs >> v[X] >> v[Y] >> v[Z];
                vertices.push(v);
            } else if (identifier == "f") {
                Size i, j, k;
                ifs >> i >> j >> k;
                triangles.emplaceBack(vertices[i - 1], vertices[j - 1], vertices[k - 1]);
            }
        }

        return Expected<Array<Triangle>>(std::move(triangles));

    } catch (const std::exception& e) {
        return makeUnexpected<Array<Triangle>>(exceptionMessage(e));
    }
}

AutoPtr<IMeshFile> getMeshFile(const Path& path) {
    const String extension = path.extension().string();
    if (extension == "ply") {
        return makeAuto<PlyFile>();
    } else if (extension == "obj") {
        return makeAuto<ObjFile>();
    } else {
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
