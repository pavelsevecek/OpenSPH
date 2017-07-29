#include "gravity/VoxelGravity.h"
#include "gravity/Moments.h"
#include "objects/finders/KdTree.h"
#include "physics/Constants.h"

NAMESPACE_SPH_BEGIN

/*VoxelGravity::VoxelGravity(const Float theta, const MultipoleOrder order)
    : finder(0.1_f)
    , thetaSqr(sqr(theta))
    , order(order) {}

VoxelGravity::VoxelGravity(const Float theta, const MultipoleOrder order, GravityLutKernel&& kernel)
    : finder(0.1_f)
    , kernel(std::move(kernel))
    , thetaSqr(sqr(theta))
    , order(order) {}

void VoxelGravity::build(ArrayView<const Vector> pos, ArrayView<const Float> masses) {
    r = pos;
    m = masses;

    // build the finder
    finder.build(r);

    // compute the moments in each cell
    const Size size = finder.getDimensionSize();
    cells.resize(pow<3>(size));
    Size i = 0;
    finder.iterate([&](ArrayView<const Size> idxs) {
        this->buildCell(cells[i], idxs);
        i++;
    });
}

Vector VoxelGravity::eval(const Size idx, Statistics& stats) {
    return this->evalImpl(r[idx], idx, stats);
}

Vector VoxelGravity::eval(const Vector& r0, Statistics& stats) {
    return this->evalImpl(r0, Size(-1), stats);
}

Vector VoxelGravity::evalImpl(const Vector& r0, const Size idx0, Statistics& UNUSED(stats)) {
    Size i = 0;
    Vector f(0._f);
    finder.iterate([&](ArrayView<const Size> idxs) {
        MARK_USED(idxs);
        MARK_USED(r0);
        MARK_USED(idx0);
        Cell& cell = cells[i++];
        if (cell.box == Box::EMPTY()) {
            return;
        }
        const Float boxSizeSqr = getSqrLength(cell.box.size());
        const Float boxDistSqr = getSqrLength(cell.box.center() - r0);
        ASSERT(isReal(boxDistSqr));
        if (!cell.box.contains(r0) && boxSizeSqr / (boxDistSqr + EPS) < thetaSqr) {
            // small node, use multipole approximation
            // f += cell.moments.order<0>().value() * kernel.grad(cell.box.center() - r0, r0[H]);
            // f += evaluateGravity(r0 - cell.com, cell.moments, order);
        } else {
            for (Size j : idxs) {
                if (idx0 == j) {
                    continue;
                }
                ASSERT(r[j][H] > 0._f, r[j][H]);
                f += m[j] * kernel.grad(r[j] - r0, r0[H]);
            }
            ASSERT(isReal(f), f);
        }
    });
    return Constants::gravity * f;
}

void VoxelGravity::buildCell(Cell& cell, ArrayView<const Size> idxs) {
    /// \todo heavily duplicated code

    if (idxs.empty()) {
        // set all moments to zero
        cell.moments.order<0>() = 0._f;
        cell.moments.order<1>() = TracelessMultipole<1>(0._f);
        cell.moments.order<2>() = TracelessMultipole<2>(0._f);
        cell.moments.order<3>() = TracelessMultipole<3>(0._f);
        return;
    }
    // compute the center of gravity
    cell.box = Box::EMPTY();
    cell.com = Vector(0._f);
    Float m_leaf = 0._f;
    LeafIndexSequence sequence(0, idxs.size(), idxs);
    for (Size i : sequence) {
        cell.com += m[i] * r[i];
        m_leaf += m[i];
        // extend the bounding box, given particle radius
        ASSERT(r[i][H] > 0._f, r[i][H]);
        const Vector dr(r[i][H] * kernel.closeRadius());
        cell.box.extend(r[i] + dr);
        cell.box.extend(r[i] - dr);
    }
    ASSERT(m_leaf > 0._f, m_leaf);
    cell.com /= m_leaf;
    ASSERT(isReal(cell.com) && getLength(cell.com) < LARGE, cell.com);

    // compute gravitational moments from individual particles
    // M0 is a sum of particle masses, M1 is a dipole moment = zero around center of mass
    ASSERT(computeMultipole<0>(r, m, cell.com, sequence).value() == m_leaf);
    const Multipole<2> m2 = computeMultipole<2>(r, m, cell.com, sequence);
    const Multipole<3> m3 = computeMultipole<3>(r, m, cell.com, sequence);

    // compute traceless tensors to reduce number of independent components
    const TracelessMultipole<2> q2 = computeReducedMultipole(m2);
    const TracelessMultipole<3> q3 = computeReducedMultipole(m3);

    // save the moments to the leaf
    cell.moments.order<0>() = m_leaf;
    cell.moments.order<1>() = TracelessMultipole<1>(0._f);
    cell.moments.order<2>() = q2;
    cell.moments.order<3>() = q3;
}
*/
NAMESPACE_SPH_END
