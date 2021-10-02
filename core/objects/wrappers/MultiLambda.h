#pragma once

/// \file MultiLambda.h
/// \brief Wrapper allowing to join multiple lambdas (or other functors) to a single object.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/Traits.h"

NAMESPACE_SPH_BEGIN

// requires using... unpacking
#ifdef SPH_CPP17

template <typename... TFuncs>
class MultiLambda : public TFuncs... {
public:
    template <typename... Ts>
    explicit MultiLambda(Ts&&... ts)
        : TFuncs(std::forward<Ts>(ts))... {}

    using TFuncs::operator()...;
};

template <typename... TFuncs>
MultiLambda<std::decay_t<TFuncs>...> makeMulti(TFuncs&&... funcs) {
    return MultiLambda<std::decay_t<TFuncs>...>(std::forward<TFuncs>(funcs)...);
}

#endif

NAMESPACE_SPH_END
