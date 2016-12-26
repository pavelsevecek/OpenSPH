#pragma once

/// Wrapper over other finder allowing to search neighbours in periodic domain.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/finders/AbstractFinder.h"

NAMESPACE_SPH_BEGIN

template <typename TFinder>
class PeriodicFinder : public Abstract::Finder {
private:
    TFinder finder;
    Abstract::Domain* domain;

public:
    template <typename... TArgs>
    PeriodicFinder(TArgs&& args)
        : finder(std::forward<TArgs>(args)...) {}

    virtual void findNeighbours(const int index,
                                const Float radius,
                                Array<NeighbourRecord>& neighbours,
                                Flags<FinderFlags> flags = EMPTY_FLAGS,
                                const Float error        = 0._f) const override {
        // seach 'regular' particles
        int n = finder(index, radius, neighbours, flags, error);

        /// now we have to move the particle, or create a new one with the same smoothing rank, but we are in
        /// const method, hm ...
        /// seems like removing const here would be the best way to do it (plus we need an array instead of arrayview)
        /// also, periodic conditions don't make much sense for 'curved' surfaces (like a spherical domain)
        /// we should only use it for a block domain, possibly for the base of a cylinder (infinite tube)
    }
};

NAMESPACE_SPH_END
