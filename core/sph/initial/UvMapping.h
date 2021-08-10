#pragma once

#include "objects/geometry/Box.h"

NAMESPACE_SPH_BEGIN

class Storage;

class IUvMapping : public Polymorphic {
public:
    virtual Array<Vector> generate(const Storage& storage) const = 0;
};

class SphericalUvMapping : public IUvMapping {
public:
    virtual Array<Vector> generate(const Storage& storage) const override;
};

class PlanarUvMapping : public IUvMapping {
public:
    virtual Array<Vector> generate(const Storage& storage) const override;
};

NAMESPACE_SPH_END
