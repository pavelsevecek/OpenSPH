#pragma once

#include "math/MathUtils.h"
#include "objects/containers/FlatMap.h"
#include "objects/containers/StaticArray.h"
#include "objects/utility/StringUtils.h"
#include "objects/wrappers/Expected.h"
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

enum class BasicDimension {
    LENGTH,
    MASS,
    TIME,
    ANGLE,
};

constexpr Size DIMENSION_CNT = 4;

// use only the units we need, we can extend it any time
class UnitDimensions {
private:
    StaticArray<int, DIMENSION_CNT> values;

public:
    UnitDimensions() = default;

    UnitDimensions(const BasicDimension& basicDimension) {
        values.fill(0);
        values[int(basicDimension)] = 1;
    }

    UnitDimensions(const UnitDimensions& other) {
        for (Size i = 0; i < DIMENSION_CNT; ++i) {
            values[i] = other.values[i];
        }
    }

    UnitDimensions(const int length, const int mass, const int time, const int angle)
        : values{ length, mass, time, angle } {}

    UnitDimensions& operator=(const UnitDimensions& other) {
        for (Size i = 0; i < DIMENSION_CNT; ++i) {
            values[i] = other.values[i];
        }
        return *this;
    }

    int& operator[](const BasicDimension dim) {
        return values[int(dim)];
    }

    int operator[](const BasicDimension dim) const {
        return values[int(dim)];
    }

    UnitDimensions& operator+=(const UnitDimensions& other) {
        for (Size i = 0; i < DIMENSION_CNT; ++i) {
            values[i] += other.values[i];
        }
        return *this;
    }

    UnitDimensions& operator-=(const UnitDimensions& other) {
        for (Size i = 0; i < DIMENSION_CNT; ++i) {
            values[i] -= other.values[i];
        }
        return *this;
    }

    friend UnitDimensions operator+(const UnitDimensions& dim1, const UnitDimensions& dim2) {
        UnitDimensions sum(dim1);
        sum += dim2;
        return sum;
    }

    friend UnitDimensions operator-(const UnitDimensions& dim1, const UnitDimensions& dim2) {
        UnitDimensions diff(dim1);
        diff -= dim2;
        return diff;
    }

    UnitDimensions operator-() const {
        UnitDimensions dims;
        for (Size i = 0; i < DIMENSION_CNT; ++i) {
            dims.values[i] = -values[i];
        }
        return dims;
    }

    UnitDimensions& operator*=(const int mult) {
        for (Size i = 0; i < DIMENSION_CNT; ++i) {
            values[i] *= mult;
        }
        return *this;
    }

    friend UnitDimensions operator*(const UnitDimensions& dim, const int mult) {
        UnitDimensions result(dim);
        result *= mult;
        return result;
    }

    friend UnitDimensions operator*(const int mult, const UnitDimensions& dim) {
        return dim * mult;
    }


    bool operator==(const UnitDimensions& other) const {
        return values == other.values;
    }

    bool operator!=(const UnitDimensions& other) const {
        return values != other.values;
    }

    static UnitDimensions dimensionless() {
        return { 0, 0, 0, 0 };
    }

    static UnitDimensions length() {
        return BasicDimension::LENGTH;
    }

    static UnitDimensions mass() {
        return BasicDimension::MASS;
    }

    static UnitDimensions time() {
        return BasicDimension::TIME;
    }

    static UnitDimensions velocity() {
        return length() - time();
    }

    static UnitDimensions acceleration() {
        return length() - 2 * time();
    }

    static UnitDimensions area() {
        return 2 * length();
    }

    static UnitDimensions volume() {
        return 3 * length();
    }

    static UnitDimensions density() {
        return mass() - volume();
    }

    static UnitDimensions numberDensity() {
        return -volume();
    }

    static UnitDimensions force() {
        return mass() + acceleration();
    }

    static UnitDimensions energy() {
        return force() + length();
    }

    static UnitDimensions energyDensity() {
        return energy() - volume();
    }

    static UnitDimensions power() {
        return energy() - time();
    }
};


class UnitSystem {
private:
    StaticArray<Float, DIMENSION_CNT> coeffs;

public:
    UnitSystem() = default;

    UnitSystem(const UnitSystem& other) {
        for (Size i = 0; i < DIMENSION_CNT; ++i) {
            coeffs[i] = other.coeffs[i];
        }
    }

    UnitSystem(const Float length, const Float mass, const Float time, const Float angle)
        : coeffs{ length, mass, time, angle } {}

    /// \brief Returns the conversion factor with a respect to the reference unit system.
    Float getFactor(const UnitDimensions& dimensions) const {
        Float factor = 1._f;
        for (Size i = 0; i < DIMENSION_CNT; ++i) {
            factor *= std::pow(coeffs[i], dimensions[BasicDimension(i)]);
        }
        return factor;
    }

    Float& operator[](const BasicDimension dim) {
        return coeffs[int(dim)];
    }

    Float operator[](const BasicDimension dim) const {
        return coeffs[int(dim)];
    }

    static UnitSystem SI() {
        return { 1._f, 1._f, 1._f, 1._f };
    }

    static UnitSystem CGS() {
        return { 0.01_f, 0.001_f, 1._f, 1._f };
    }
};

extern UnitSystem CODE_UNITS;

/// \todo better name
class Unit {
private:
    Float data;
    UnitDimensions dimensions;

public:
    Unit() = default;

    /// \brief Creates a unit, given its value, dimensions and a unit system in which the value is expressed.
    Unit(const Float& value, const UnitDimensions& dimensions, const UnitSystem& system)
        : dimensions(dimensions) {
        // convert value to code units
        const Float conversion = system.getFactor(dimensions) / CODE_UNITS.getFactor(dimensions);
        this->data = conversion * value;
    }

    /// \brief Returns the value in given unit system.
    Float value(const UnitSystem& system) const {
        return data / system.getFactor(dimensions);
    }

    UnitDimensions dimension() const {
        return dimensions;
    }

    /// Arithmetics
    Unit& operator+=(const Unit& other) {
        SPH_ASSERT(dimensions == other.dimensions);
        data += other.data;
        return *this;
    }

    Unit& operator-=(const Unit& other) {
        SPH_ASSERT(dimensions == other.dimensions);
        data -= other.data;
        return *this;
    }

    Unit operator-() const {
        Unit neg;
        neg.dimensions = dimensions;
        neg.data = -data;
        return neg;
    }

    Unit& operator*=(const Float f) {
        data *= f;
        return *this;
    }

    Unit& operator*=(const Unit& other) {
        data *= other.data;
        dimensions += other.dimensions;
        return *this;
    }

    Unit& operator/=(const Unit& other) {
        data /= other.data;
        dimensions -= other.dimensions;
        return *this;
    }

    bool operator==(const Unit& other) const {
        SPH_ASSERT(dimensions == other.dimensions);
        return data == other.data;
    }

    bool operator!=(const Unit& other) const {
        return !(*this == other);
    }

    bool operator<(const Unit& other) const {
        SPH_ASSERT(dimensions == other.dimensions);
        return data < other.data;
    }

    bool operator<=(const Unit& other) const {
        SPH_ASSERT(dimensions == other.dimensions);
        return data <= other.data;
    }

    bool operator>(const Unit& other) const {
        return !(*this <= other);
    }

    bool operator>=(const Unit& other) const {
        return !(*this < other);
    }

    /// Utility functions

    /// \brief Prints in the most suitable units (so that the value is as close to 1 as possible).
    ///
    /// Can be only used with base units.
    /*std::string human() const {
        const Float ratio;
        for (UnitDescriptor& desc : UNITS) {
            if (desc.value.type() == this->type()) {
            }
        }
    }*/

    friend std::ostream& operator<<(std::ostream& stream, const Unit& u);

    friend Unit pow(const Unit& u, const int power) {
        Unit result;
        result.data = std::pow(u.data, power);
        result.dimensions = u.dimensions * power;
        return result;
    }

    friend bool almostEqual(const Unit& u1, const Unit& u2, const Float eps) {
        SPH_ASSERT(u1.dimensions == u2.dimensions);
        return Sph::almostEqual(u1.data, u2.data, eps);
    }

    static Unit dimensionless(const Float value) {
        return Unit(value, UnitDimensions(0, 0, 0, 0), UnitSystem::SI());
    }

    static Unit kilogram(const Float value) {
        return Unit(value, BasicDimension::MASS, UnitSystem::SI());
    }

    static Unit meter(const Float value) {
        return Unit(value, BasicDimension::LENGTH, UnitSystem::SI());
    }

    static Unit second(const Float value) {
        return Unit(value, BasicDimension::TIME, UnitSystem::SI());
    }

    static Unit radian(const Float value) {
        return Unit(value, BasicDimension::ANGLE, UnitSystem::SI());
    }
};

inline Unit operator+(const Unit& u1, const Unit& u2) {
    Unit sum(u1);
    sum += u2;
    return sum;
}

inline Unit operator-(const Unit& u1, const Unit& u2) {
    Unit diff(u1);
    diff -= u2;
    return diff;
}

inline Unit operator*(const Unit& u, const Float f) {
    Unit mult(u);
    mult *= f;
    return mult;
}

inline Unit operator*(const Float f, const Unit& u) {
    return u * f;
}

inline Unit operator*(const Unit& u1, const Unit& u2) {
    Unit mult(u1);
    mult *= u2;
    return mult;
}

inline Unit operator/(const Unit& u1, const Unit& u2) {
    Unit div(u1);
    div /= u2;
    return div;
}


/*enum class UnitEnum {
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

    /// Derived reciprocal time units
    REVS_PER_DAY,
    HERTZ,
};*/


INLINE Unit operator"" _kg(long double value) {
    return Unit::kilogram(Float(value));
}
INLINE Unit operator"" _g(long double value) {
    return 1.e-3_kg * Float(value);
}
INLINE Unit operator"" _m(long double value) {
    return Unit::meter(Float(value));
}
INLINE Unit operator"" _cm(long double value) {
    return 0.01_m * Float(value);
}
INLINE Unit operator"" _mm(long double value) {
    return 1.e-3_m * Float(value);
}
INLINE Unit operator"" _km(long double value) {
    return 1.e3_m * Float(value);
}
INLINE Unit operator"" _s(long double value) {
    return Unit::second(Float(value));
}
INLINE Unit operator"" _rad(long double value) {
    return Unit::radian(Float(value));
}
INLINE Unit operator"" _mps(long double value) {
    return Unit(Float(value), UnitDimensions::velocity(), UnitSystem::SI());
}

Expected<Unit> parseUnit(const std::string& text);

/// Expected format: kg^3 m s^-1
/// No * or / symbols allowed, used powers
/*static Expected<Unit> parseUnit(const std::string& text) {
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
}*/

NAMESPACE_SPH_END
