#pragma once

#include "common/Assert.h"
#include "physics/Constants.h"

NAMESPACE_SPH_BEGIN

class Unit {


    /// Expected format: kg^3 m s^-1
    /// No * or / symbols allowed, used powers
    static Expected<Unit> parse(const std::string& text) {
        // unit symbols can only contains letters (both lowercase and uppercase) and underscores
        Size idx0 = 0;
        for (Size i = 0; i < text.size(); ++i) {
            if ((text[i] >= 'a' && text[i] <= 'z') || (text[i] >= 'A' && text[i] <= 'Z') || text[i] == '_') {
                continue;
            }
            std::string symbol = text.substr(idx0, i - idx0);
            UnitDescriptor* ptr = std::find_if(std::begin(UNITS),
                std::end(UNITS),
                [&symbol](UnitDescriptor& desc) { return desc.symbol == symbol; });
            if (ptr != std::end(UNITS)) {
                // symbol
            } else {
                return makeUnexpected<Unit>("Unexpected unit symbol: " + symbol);
            }

            if (text[i] == "^") {
                // make power
            }
        }
    }

    /// Prints in the most suitable units (so that the value is as close to 1 as possible). Can be only used
    /// with base units.
    std::string human() const {
        const Float ratio;
        for (UnitDescriptor& desc : UNITS) {
            if (desc.value.type() == this->type()) {
            }
        }
    }
};

enum class UnitEnum {
    /// SI units
    KILOGRAM,
    METER,
    SECOND,
    KELVIN,

    /// Derived mass unit
    GRAM,
    SOLAR_MASS,
    EARTH_MASS,

    /// Derived length units
    MILLIMETER,
    CENTIMETER,
    KILOMETER,
    AU,

    /// Derived time units
    MINUTE,
    HOUR,
    DAY,
    YEAR,
};

struct UnitDescriptor {
    UnitEnum id;
    std::string symbol;
    Unit value;
};

UnitDescriptor UNITS[] = {
    { UnitEnum::KILOGRAM, "kg", 1._kg },
    { UnitEnum::GRAM, "g", 1.e-3_kg },
    { UnitEnum::SOLAR_MASS, "M_sun", Constants::M_sun },
    { UnitEnum::EARTH_MASS, "M_earth", Constants::M_earth },

    { UnitEnum::METER, "m", 1._m },
    { UnitEnum::MILLIMETER, "mm", 1.e-3_m },
    { UnitEnum::CENTIMETER, "cm", 1.e-2_m },
    { UnitEnum::KILOMETER, "kg", 1.e3_m },
    { UnitEnum::AU, "au", Constants::au },

    { UnitEnum::SECOND, "s", 1._s },
    { UnitEnum::MINUTE, "min", 60._s },
    { UnitEnum::HOUR, "h", 3600._s },
    { UnitEnum::DAY, "d", 86400._s },
    { UnitEnum::YEAR, "y", 31556926._s },

    { UnitEnum::KELVIN, "K", 1._K },
};

NAMESPACE_SPH_END
