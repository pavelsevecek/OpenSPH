#include "physics/Units.h"

NAMESPACE_SPH_BEGIN

UnitSystem UnitSystem::codeUnits = UnitSystem::SI();

UnitParser::UnitParser()
    : list({ { 1._m, "m" },
          { 1._cm, "cm" },
          { 1._mm, "mm" },
          { 1._km, "km" },
          { 1._s, "s" },
          { 1._ms, "ms" },
          { 1._min, "min" },
          { 1._h, "h" },
          { 1._kg, "kg" },
          { 1._g, "g" } }) {}


NAMESPACE_SPH_END
