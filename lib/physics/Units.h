#pragma once

#include "common/Assert.h"
#include "physics/Constants.h"

NAMESPACE_SPH_BEGIN

/// There are three different unit systems in the code:
/// 1) Code units
///     Currently selected unit system, used for actual computation in the code. Or more precisely, we don't
///     use runtime dimensional analysis in the actual simulation for performance reasons, we convert the
///     input quantities (values and units) into floats or doubles using the selected code units. These units
///     determine the accuracy of the code; if a nanometer is selected as a unit of length in a simulation of
///     protoplanetary disk, you can expect a loss of precision. Select the code units so that the typical
///     values of quantities in your simulation correspond to the unit values of code units.
///
/// 2) Reference units
///     Unit system used as a reference for all conversions. Used for convenience as we do not want to define
///     conversions between each pair of unit systems; for each unit system, we simply define its relation to
///     the reference units. This is always SI, for simplicity.
///
/// 3) Input/output units
///     Selected units of input or output values.

class Dimensions {
    // use only the units we need, we can extend it any time
    StaticArray<int, 3> f;
};

enum class BasicDimension {
    LENGTH,
    MASS,
    TIME,
};

constexpr Size DIMENSION_CNT = 3;

class UnitSystem : public Noncopyable {
private:
    StaticArray<Float, DIMENSION_CNT> coeffs;

public:
    template <typename Type>
    Unit<Type> getUnit(const Type& value, const Dimensions& dimensions) {}

    static UnitSystem SI() {
        return { 1._f, 1._f, 1._f };
    }

    static UnitSystem CGS() {
        return { 0.01_f, 0.001_f, 1._f };
    }
};

/// \todo better name
template <typename Type>
class Unit {
private:
    Type value;

public:
    Unit() = default;

    Unit(const Type& value, const UnitSystem& system) {}

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
