#include "physics/Units.h"

NAMESPACE_SPH_BEGIN

UnitSystem UnitSystem::codeUnits = UnitSystem::SI();

Unit::Unit(const Float value, const UnitSystem& system, const Dimensions& dimensions)
    : data(value / (system.conversion(dimensions) / UnitSystem::code().conversion(dimensions)))
    , dimensions(dimensions) {}

Float Unit::value(const UnitSystem& system) const {
    return data * (system.conversion(dimensions) / UnitSystem::code().conversion(dimensions));
}

NAMESPACE_SPH_END
