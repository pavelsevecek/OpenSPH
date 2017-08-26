#pragma once

#include "gravity/IGravity.h"
#include "objects/finders/Voxel.h"
#include "sph/kernel/GravityKernel.h"

NAMESPACE_SPH_BEGIN

enum class MultipoleOrder;

/*class VoxelGravity : public IGravity {
protected:
    /// Source data
    ArrayView<const Vector> r;
    ArrayView<const Float> m;

    VoxelFinder finder;

    struct Cell {
        MultipoleExpansion<3> moments;

        Vector com;

        Box box;
    };


    Array<Cell> cells;

    /// Kernel used to evaluate gravity of close particles
    GravityLutKernel kernel;

    /// Opening angle for multipole approximation (in radians)
    Float thetaSqr;

    /// Order of multipole approximation
    MultipoleOrder order;

public:
    /// Constructs the gravity assuming point-like particles (with zero radius).
    /// \param theta Opening angle; lower value means higher precision, but slower computation
    /// \param order Order of multipole approximation
    VoxelGravity(const Float theta, const MultipoleOrder order);

    /// Constructs the gravity with given smoothing kernel
    /// \param theta Opening angle; lower value means higher precision, but slower computation
    /// \param order Order of multipole approximation
    VoxelGravity(const Float theta, const MultipoleOrder order, GravityLutKernel&& kernel);

    /// Masses of particles must be strictly positive, otherwise center of mass would be undefined.
    virtual void build(const Storage& storage) override;

    virtual Vector eval(const Size idx, Statistics& stats) override;

    virtual Vector eval(const Vector& r0, Statistics& stats) override;

private:
    void buildCell(Cell& cell, ArrayView<const Size> idxs);

    Vector evalImpl(const Vector& r0, const Size idx0, Statistics& stats);
};
*/

NAMESPACE_SPH_END
