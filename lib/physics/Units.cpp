#include "physics/Units.h"

NAMESPACE_SPH_BEGIN

UnitSystem UnitSystem::codeUnits = UnitSystem::SI();

Unit::Unit(const Float value, const UnitSystem& system, const Dimensions& dimensions)
    : data(value / (system.conversion(dimensions) / UnitSystem::code().conversion(dimensions)))
    , dims(dimensions) {}

Float Unit::value(const UnitSystem& system) const {
    return data * (system.conversion(dims) / UnitSystem::code().conversion(dims));
}

NAMESPACE_SPH_END
