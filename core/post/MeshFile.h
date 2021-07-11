#pragma once

#include "io/Path.h"
#include "objects/finders/Order.h"
#include "objects/wrappers/AutoPtr.h"
#include "objects/wrappers/Expected.h"
#include "objects/wrappers/Outcome.h"
#include "post/Mesh.h"

NAMESPACE_SPH_BEGIN

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

/// \brief Simple text format containing mesh vertices and triangle indices.
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

/// \brief Text format containing mesh vertices (prefixed by 'v') and triangle indices (prefixed by 'f')
class ObjFile : public IMeshFile {
public:
    virtual Outcome save(const Path& path, ArrayView<const Triangle> triangles) override;

    virtual Expected<Array<Triangle>> load(const Path& path) override;
};

/// \brief Deduces mesh type from extension of given path.
AutoPtr<IMeshFile> getMeshFile(const Path& path);

NAMESPACE_SPH_END
