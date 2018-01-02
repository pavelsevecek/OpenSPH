#pragma once

/// \file PeriodicFinder.h
/// \brief Wrapper over other finder allowing to search neighbours in periodic domain.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/finders/INeighbourFinder.h"

NAMESPACE_SPH_BEGIN

template <typename TFinder>
class PeriodicFinder : public INeighbourFinder {
private:
    TFinder finder;
    IDomain* domain;

public:
    template <typename... TArgs>
    PeriodicFinder(TArgs&& args)
        : finder(std::forward<TArgs>(args)...) {}

    virtual void findNeighbours(const int index,
        const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlag> flags = EMPTY_FLAGS,
        const Float error = 0._f) const override {
        // seach 'regular' particles
        int n = finder(index, radius, neighbours, flags, error);

        /// now we have to move the particle, or create a new one with the same smoothing rank, but we are in
        /// const method, hm ...
        /// seems like removing const here would be the best way to do it (plus we need an array instead of
        /// arrayview)
        /// also, periodic conditions don't make much sense for 'curved' surfaces (like a spherical domain)
        /// we should only use it for a block domain, possibly for the base of a cylinder (infinite tube)
    }
};

NAMESPACE_SPH_END
