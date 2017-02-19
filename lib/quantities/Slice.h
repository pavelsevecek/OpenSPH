#pragma once

/// Heterogeneous array of quantities corresponding to single particle. Can be viewed as a 'slice' through
/// storage, extracting quantity values from arrays of quantities.
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/wrappers/Value.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

class Slice {
private:
    Array<Value> values;
};

NAMESPACE_SPH_END
