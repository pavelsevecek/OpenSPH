#pragma once

#include "system/Logger.h"

/// Convenient object for storing scalar, vector or tensor value. Wrapper provides basic arithmetic operations
/// and generic output method. As all operation are implemented using virtual function, this object should not
/// be used in performance critical code.
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

NAMESPACE_SPH_BEGIN

class Value {
public:
    virtual void print(Abstract::Logger& logger) const = 0;
};

class ScalarValue : public Value {

};

NAMESPACE_SPH_END
