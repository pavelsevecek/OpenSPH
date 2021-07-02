#pragma once

/// \file AdaptiveGrid.h
/// \brief Finder projecting a non-uniform grid on particles
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/finders/NeighbourFinder.h"

NAMESPACE_SPH_BEGIN

/// \brief Finder projecting a non-uniform grid on particles
///
/// The cell size depends on smoothing lengths of the particles.
/// Inspired by \cite Batra_Zhang_2007
class AdaptiveGridFinder : public INeighbourFinder {
protected:
    virtual void buildImpl(ArrayView<const Vector> points) override;

    virtual void rebuildImpl(ArrayView<const Vector> points) override;

public:
    explicit AdaptiveGridFinder(const Float relativeCellCnt = 1);

    virtual Size findNeighbours(const Size index,
        const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlag> flags = EMPTY_FLAGS,
        const Float error = 0._f) const override;

    virtual Size findNeighbours(const Vector& position,
        const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlag> flags = EMPTY_FLAGS,
        const Float error = 0._f) const override;
};

NAMESPACE_SPH_END
