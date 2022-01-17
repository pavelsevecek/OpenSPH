#pragma once

#include "io/Output.h"

NAMESPACE_SPH_BEGIN

/// \brief Saves particle data as OpenVDB grid.
class VdbOutput : public IOutput {
private:
    float surfaceLevel;

public:
    VdbOutput(const OutputFile& fileMask, const Float surfaceLevel = 0.13);
    ~VdbOutput();

    virtual Expected<Path> dump(const Storage& storage, const Statistics& stats) override;
};

NAMESPACE_SPH_END
