#pragma once

/// Run-time dimensional analysis
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "core/Globals.h"
#include "objects/containers/StaticArray.h"

NAMESPACE_SPH_BEGIN


static constexpr int DIMENSION_CNT = 7;

/// \todo so far last 4 dimensions are not being used at all, possibly remove, unnecessary overhead

struct Dimensions {
private:
    // let's hope nobody will use higher power than 127 ...
    StaticArray<int8_t, DIMENSION_CNT> dims;

    Dimensions(StaticArray<int8_t, DIMENSION_CNT>&& other)
        : dims(std::move(other)) {}

public:
    enum { MASS = 0, LENGTH = 1, TIME = 2, CHARGE = 3, TEMPERATURE = 4, INTENSITY = 5, ANGLE = 6 };

    Dimensions(const int8_t mass = 0,
        const int8_t length = 0,
        const int8_t time = 0,
        const int8_t charge = 0,
        const int8_t temperature = 0,
        const int8_t intensity = 0,
        const int8_t angle = 0)
        : dims{ mass, length, time, charge, temperature, intensity, angle } {}

    Dimensions(const Dimensions& other)
        : dims(other.dims.clone()) {}

    Dimensions& operator=(const Dimensions& other) {
        dims = other.dims.clone();
        return *this;
    }

    int8_t operator[](const Size idx) const {
        return dims[idx];
    }

    bool operator==(const Dimensions& other) const {
        for (Size i = 0; i < DIMENSION_CNT; ++i) {
            if (dims[i] != other.dims[i]) {
                return false;
            }
        }
        return true;
    }

    friend Dimensions operator+(const Dimensions& d1, const Dimensions& d2) {
        StaticArray<int8_t, DIMENSION_CNT> sum;
        for (Size i = 0; i < DIMENSION_CNT; ++i) {
            sum[i] = d1.dims[i] + d2.dims[i];
        }
        return sum;
    }

    friend Dimensions operator-(const Dimensions& d1, const Dimensions& d2) {
        StaticArray<int8_t, DIMENSION_CNT> diff;
        for (Size i = 0; i < DIMENSION_CNT; ++i) {
            diff[i] = d1.dims[i] - d2.dims[i];
        }
        return diff;
    }

    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const Dimensions& dimensions) {
        /// \todo printing should depend on selected unit system
        stream << print("kg", dimensions[MASS]) << print("m", dimensions[LENGTH])
               << print("s", dimensions[TIME]) << print("C", dimensions[CHARGE])
               << print("K", dimensions[TEMPERATURE]) << print("cd", dimensions[INTENSITY])
               << print("rad", dimensions[ANGLE]);
        return stream;
    }


    /// Base units
    static Dimensions dimensionless() {
        return Dimensions();
    }
    static Dimensions mass() {
        return Dimensions(1, 0, 0, 0, 0, 0, 0);
    }
    static Dimensions length() {
        return Dimensions(0, 1, 0, 0, 0, 0, 0);
    }
    static Dimensions time() {
        return Dimensions(0, 0, 1, 0, 0, 0, 0);
    }
    static Dimensions charge() {
        return Dimensions(0, 0, 0, 1, 0, 0, 0);
    }
    static Dimensions temperature() {
        return Dimensions(0, 0, 0, 0, 1, 0, 0);
    }
    static Dimensions intensity() {
        return Dimensions(0, 0, 0, 0, 0, 1, 0);
    }
    static Dimensions angle() {
        return Dimensions(0, 0, 0, 0, 0, 0, 1);
    }

    /// Derived units
    static Dimensions area() {
        return Dimensions(0, 2, 0, 0, 0, 0, 0);
    }
    static Dimensions volume() {
        return Dimensions(0, 3, 0, 0, 0, 0, 0);
    }
    static Dimensions density() {
        return Dimensions(1, -3, 0, 0, 0, 0, 0);
    }
    static Dimensions velocity() {
        return Dimensions(0, 1, -1, 0, 0, 0, 0);
    }

private:
    INLINE static std::string print(const std::string& s, const int8_t m) {
        switch (m) {
        case 0:
            return "";
        case 1:
            return s;
        default:
            return s + "^" + std::to_string(m);
        }
    }
};


/// Code uses three systems of units:
/// - reference system, used to provide a base for all transformations of units. In the code, the reference
///   system is SI.
/// - code units, actual system of units used within the run. This does not affect input nor output values, at
///   least not directly, selected system of units can affect the output precision, though.
/// - output units, unit system to which all output quantities are transformed.
class UnitSystem {
private:
    StaticArray<Float, DIMENSION_CNT> values;

    static UnitSystem codeUnits;

public:
    /// Constructs unit system given transformation coefficients from SI. Default arguments correspond to SI.
    UnitSystem(Float mass = 1._f,
        Float length = 1._f,
        Float time = 1._f,
        Float charge = 1._f,
        Float temperature = 1._f,
        Float intensity = 1._f,
        Float angle = 1._f)
        : values{ mass, length, time, charge, temperature, intensity, angle } {}

    UnitSystem(const UnitSystem& other)
        : values(other.values.clone()) {}

    UnitSystem& operator=(const UnitSystem& other) {
        values = other.values.clone();
        return *this;
    }

    static UnitSystem SI() {
        return UnitSystem();
    }

    static UnitSystem CGS() {
        return UnitSystem(0.001_f, 0.01_f, 1._f, 3.336e-10_f);
    }

    /// Returns the reference to internal code units. Can be used to change units to something other than SI.
    /// \note This is a global state of the program. Changing the value will invalidate all currently existing
    /// unit objects as their values still refer to the old system of units. Internal system of units should
    /// only be called at the startup of the program before any other parameter is set. It should never be
    /// changed during or even after the run.
    static UnitSystem& code() {
        return codeUnits;
    }

    Float conversion(const Dimensions& dimensions) const {
        Float coeff = 1._f;
        for (Size i = 0; i < DIMENSION_CNT; ++i) {
            coeff *= std::pow(values[i], -dimensions[i]);
        }
        return coeff;
    }

    Float operator[](const Size idx) const {
        return values[idx];
    }
};

/// Base class holding a value and unit of physical quantity.
/// Provides dimensional analysis and conversions between unit systems.
/// Note that this adds significant overhead, both CPU-wise and memory-wise.
class Unit {
private:
    Float data;
    Dimensions dims;

    Unit(const Float value, const Dimensions& dimensions)
        : data(value)
        , dims(dimensions) {}

public:
    /// Default constructor, does not initialize value. Unit must be set by assign operator to avoid undefined
    /// behavior.
    Unit() = default;

    /// Constructs unit given value and unit system.
    Unit(const Float data, const UnitSystem& system, const Dimensions& dimensions);

    /// Assigns value and dimensions of other unit.
    Unit& operator=(const Unit& other) {
        data = other.data;
        dims = other.dims;
        return *this;
    }

    /// Returns the value for given unit system.
    Float value(const UnitSystem& system) const;

    /// Returns the dimensions of the unit.
    const Dimensions& dimensions() const {
        return dims;
    }

    /// Returns true if and only if both values and dimensions of both units match.
    bool operator==(const Unit& other) {
        return data == other.data && dims == other.dims;
    }

    INLINE friend bool dimensionsMatch(const Unit& u1, const Unit& u2) {
        return u1.dims == u2.dims;
    }

    INLINE friend Unit operator+(const Unit& u1, const Unit& u2) {
        ASSERT(u1.dims == u2.dims);
        return { u1.data + u2.data, u1.dims };
    }

    INLINE friend Unit operator-(const Unit& u1, const Unit& u2) {
        ASSERT(u1.dims == u2.dims);
        return { u1.data - u2.data, u1.dims };
    }

    INLINE friend Unit operator*(const Unit& u, const Float f) {
        return { u.data * f, u.dims };
    }

    INLINE friend Unit operator*(const Float f, const Unit& u) {
        return { u.data * f, u.dims };
    }

    INLINE friend Unit operator*(const Unit& u1, const Unit& u2) {
        return { u1.data * u2.data, u1.dims + u2.dims };
    }

    INLINE friend Unit operator/(const Unit& u, const Float f) {
        return { u.data / f, u.dims };
    }

    INLINE friend Unit operator/(const Unit& u1, const Unit& u2) {
        return { u1.data / u2.data, u1.dims - u2.dims };
    }

    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const Unit& unit) {
        /// \todo this should use custom output units, not SI
        const Float conversion = UnitSystem::codeUnits.conversion(unit.dims);
        if (conversion != 1._f) {
            stream << unit.data << " [" << 1._f / conversion << unit.dims << "]";
        } else {
            stream << unit.data << " [" << unit.dims << "]";
        }
        return stream;
    }
};

/// Unit literals

/// Length
INLINE Unit operator"" _m(const long double value) {
    return Unit(value, UnitSystem::SI(), Dimensions::length());
}
INLINE Unit operator"" _cm(const long double value) {
    return Unit(1.e-2_f * value, UnitSystem::SI(), Dimensions::length());
}
INLINE Unit operator"" _mm(const long double value) {
    return Unit(1.e-3_f * value, UnitSystem::SI(), Dimensions::length());
}
INLINE Unit operator"" _km(const long double value) {
    return Unit(1.e3_f * value, UnitSystem::SI(), Dimensions::length());
}

/// Time
INLINE Unit operator"" _s(const long double value) {
    return Unit(value, UnitSystem::SI(), Dimensions::time());
}
INLINE Unit operator"" _ms(const long double value) {
    return Unit(1.e-3_f * value, UnitSystem::SI(), Dimensions::time());
}
INLINE Unit operator"" _min(const long double value) {
    return Unit(60._f * value, UnitSystem::SI(), Dimensions::time());
}
INLINE Unit operator"" _h(const long double value) {
    return Unit(3600._f * value, UnitSystem::SI(), Dimensions::time());
}

/// Mass
INLINE Unit operator"" _kg(const long double value) {
    return Unit(value, UnitSystem::SI(), Dimensions::mass());
}
INLINE Unit operator"" _g(const long double value) {
    return Unit(1.e-3_f * value, UnitSystem::SI(), Dimensions::mass());
}


NAMESPACE_SPH_END
