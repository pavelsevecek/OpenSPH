#pragma once

/// \file Roots.h
/// \brief Finding roots of functions
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/wrappers/Optional.h"
#include "objects/wrappers/Range.h"

NAMESPACE_SPH_BEGIN


/// Returns a root of a R->R function on given range. If there is no root or the root cannot be found, returns
/// NOTHING. For functions with multiple roots, returns one of them; the selection of such root is not
/// specified.
template <typename TFunctor>
INLINE Optional<Float> getRoot(const TFunctor& functor, const Range& range, const Float eps = EPS) {
    Range r = range;
    if (functor(r.lower()) * functor(r.upper()) > 0._f) { // same sign
        return NOTHING;
    }
    while (r.size() > eps * range.size()) {
        Float x = r.center();
        if (functor(x) * functor(r.upper()) > 0._f) {
            r = Range(r.lower(), x);
        } else {
            r = Range(x, r.upper());
        }
    }
    return r.center();
}

NAMESPACE_SPH_END
