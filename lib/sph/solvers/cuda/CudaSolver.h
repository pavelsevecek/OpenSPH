#pragma once

#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

class CudaSolver : public ISolver {
public:
    virtual void integrate(Storage& UNUSED(storage), Statistics& UNUSED(stats)) override;

    virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
};

NAMESPACE_SPH_END
