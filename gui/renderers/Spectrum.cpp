#include "gui/renderers/Spectrum.h"
#include "objects/wrappers/Lut.h"
#include "physics/Constants.h"

NAMESPACE_SPH_BEGIN

class Xyz {
private:
    BasicVector<float> data;

public:
    Xyz() = default;

    Xyz(const float x, const float y, const float z)
        : data(x, y, z) {}

    float& operator[](const int index) {
        SPH_ASSERT(index < 3);
        return data[index];
    }

    Xyz& operator+=(const Xyz& other) {
        *this = *this + other;
        return *this;
    }

    Xyz operator+(const Xyz& other) const {
        Xyz sum;
        sum.data = data + other.data;
        return sum;
    }

    Xyz operator*(const float factor) const {
        Xyz result;
        result.data = data * factor;
        return result;
    }

    Xyz operator/(const float factor) const {
        Xyz result;
        result.data = data / factor;
        return result;
    }

    float& x() {
        return data[0];
    }

    float x() const {
        return data[0];
    }

    float& y() {
        return data[1];
    }

    float y() const {
        return data[1];
    }

    float& z() {
        return data[2];
    }

    float z() const {
        return data[2];
    }
};

inline Rgba xyzToRgb(const Xyz& c) {
    const float r = 3.2404542f * c.x() - 1.5371385f * c.y() - 0.4985314f * c.z();
    const float g = -0.9692660f * c.x() + 1.8760108f * c.y() + 0.0415560f * c.z();
    const float b = 0.0556434f * c.x() - 0.2040259f * c.y() + 1.0572252f * c.z();
    return Rgba(r, g, b);
}

static Lut<Xyz, float> wavelengthToXyzLut = Lut<Xyz, float>(Interval(380, 780),
    Array<Xyz>{
        { 0.0014f, 0.0000f, 0.0065f },
        { 0.0022f, 0.0001f, 0.0105f },
        { 0.0042f, 0.0001f, 0.0201f },
        { 0.0077f, 0.0002f, 0.0362f },
        { 0.0143f, 0.0004f, 0.0679f },
        { 0.0232f, 0.0006f, 0.1102f },
        { 0.0435f, 0.0012f, 0.2074f },
        { 0.0776f, 0.0022f, 0.3713f },
        { 0.1344f, 0.0040f, 0.6456f },
        { 0.2148f, 0.0073f, 1.0391f },
        { 0.2839f, 0.0116f, 1.3856f },
        { 0.3285f, 0.0168f, 1.6230f },
        { 0.3483f, 0.0230f, 1.7471f },
        { 0.3481f, 0.0298f, 1.7826f },
        { 0.3362f, 0.0380f, 1.7721f },
        { 0.3187f, 0.0480f, 1.7441f },
        { 0.2908f, 0.0600f, 1.6692f },
        { 0.2511f, 0.0739f, 1.5281f },
        { 0.1954f, 0.0910f, 1.2876f },
        { 0.1421f, 0.1126f, 1.0419f },
        { 0.0956f, 0.1390f, 0.8130f },
        { 0.0580f, 0.1693f, 0.6162f },
        { 0.0320f, 0.2080f, 0.4652f },
        { 0.0147f, 0.2586f, 0.3533f },
        { 0.0049f, 0.3230f, 0.2720f },
        { 0.0024f, 0.4073f, 0.2123f },
        { 0.0093f, 0.5030f, 0.1582f },
        { 0.0291f, 0.6082f, 0.1117f },
        { 0.0633f, 0.7100f, 0.0782f },
        { 0.1096f, 0.7932f, 0.0573f },
        { 0.1655f, 0.8620f, 0.0422f },
        { 0.2257f, 0.9149f, 0.0298f },
        { 0.2904f, 0.9540f, 0.0203f },
        { 0.3597f, 0.9803f, 0.0134f },
        { 0.4334f, 0.9950f, 0.0087f },
        { 0.5121f, 1.0000f, 0.0057f },
        { 0.5945f, 0.9950f, 0.0039f },
        { 0.6784f, 0.9786f, 0.0027f },
        { 0.7621f, 0.9520f, 0.0021f },
        { 0.8425f, 0.9154f, 0.0018f },
        { 0.9163f, 0.8700f, 0.0017f },
        { 0.9786f, 0.8163f, 0.0014f },
        { 1.0263f, 0.7570f, 0.0011f },
        { 1.0567f, 0.6949f, 0.0010f },
        { 1.0622f, 0.6310f, 0.0008f },
        { 1.0456f, 0.5668f, 0.0006f },
        { 1.0026f, 0.5030f, 0.0003f },
        { 0.9384f, 0.4412f, 0.0002f },
        { 0.8544f, 0.3810f, 0.0002f },
        { 0.7514f, 0.3210f, 0.0001f },
        { 0.6424f, 0.2650f, 0.0000f },
        { 0.5419f, 0.2170f, 0.0000f },
        { 0.4479f, 0.1750f, 0.0000f },
        { 0.3608f, 0.1382f, 0.0000f },
        { 0.2835f, 0.1070f, 0.0000f },
        { 0.2187f, 0.0816f, 0.0000f },
        { 0.1649f, 0.0610f, 0.0000f },
        { 0.1212f, 0.0446f, 0.0000f },
        { 0.0874f, 0.0320f, 0.0000f },
        { 0.0636f, 0.0232f, 0.0000f },
        { 0.0468f, 0.0170f, 0.0000f },
        { 0.0329f, 0.0119f, 0.0000f },
        { 0.0227f, 0.0082f, 0.0000f },
        { 0.0158f, 0.0057f, 0.0000f },
        { 0.0114f, 0.0041f, 0.0000f },
        { 0.0081f, 0.0029f, 0.0000f },
        { 0.0058f, 0.0021f, 0.0000f },
        { 0.0041f, 0.0015f, 0.0000f },
        { 0.0029f, 0.0010f, 0.0000f },
        { 0.0020f, 0.0007f, 0.0000f },
        { 0.0014f, 0.0005f, 0.0000f },
        { 0.0010f, 0.0004f, 0.0000f },
        { 0.0007f, 0.0002f, 0.0000f },
        { 0.0005f, 0.0002f, 0.0000f },
        { 0.0003f, 0.0001f, 0.0000f },
        { 0.0002f, 0.0001f, 0.0000f },
        { 0.0002f, 0.0001f, 0.0000f },
        { 0.0001f, 0.0000f, 0.0000f },
        { 0.0001f, 0.0000f, 0.0000f },
        { 0.0001f, 0.0000f, 0.0000f },
        { 0.0000f, 0.0000f, 0.0000f },
    });

/// \brief Returns wavelength of maximum emission for given temperature, according to Wien's law.
inline float getMaxEmissionWavelength(const float temperature) {
    constexpr float b = 2.8977729e-3f; /// \todo maybe move to Constants.h
    return b / temperature;
}

/// \brief Planck law
///
/// \param wavelength Wavelength in nanometers
/// \param temperature Temperature in Kelvins
inline float spectralRadiance(const float wavelength, const float temperature) {
    constexpr Float factor1 = 1.e45 * 2._f * Constants::planckConstant * sqr(Constants::speedOfLight);
    constexpr Float factor2 =
        Constants::planckConstant * Constants::speedOfLight / Constants::boltzmann * 1.e9_f;
    const Float denom = exp(factor2 / (wavelength * temperature)) - 1._f;
    SPH_ASSERT(isReal(denom));
    const Float result = factor1 / pow<5>(wavelength) * 1._f / denom;
    SPH_ASSERT(isReal(result));
    return float(result);
}

inline Xyz getBlackBodyColor(const float temperature) {
    const Interval range = wavelengthToXyzLut.getRange();
    Xyz result(0.f, 0.f, 0.f);
    float weight = 0.f;
    for (float wavelength = float(range.lower()); wavelength < range.upper(); wavelength += 5.f) {
        const Xyz xyz = wavelengthToXyzLut(wavelength);
        const float B = spectralRadiance(wavelength, temperature);
        result += xyz * B;
        weight += B;
    }
    SPH_ASSERT(weight > 0.f);
    return result / weight;
}

Palette getBlackBodyPalette(const Interval range) {
    Array<Palette::Point> points(256);
    for (Size i = 0; i < points.size(); ++i) {
        const float temperature = float(range.lower()) + float(i) / (points.size() - 1) * float(range.size());
        const Xyz xyz = getBlackBodyColor(temperature);

        points[i].value = float(i) / (points.size() - 1);
        const Rgba color = xyzToRgb(xyz);
        // normalize so that max component is 1
        points[i].color = color / max(color.r(), color.g(), color.b());
    }
    return Palette(std::move(points));
}

Palette getEmissionPalette(const Interval range) {
    const float draperPoint = 798.f;
    const float pureEmissionColor = draperPoint * 1.5f;
    const Rgba darkColor = Rgba::gray(0.2f);
    Array<Palette::Point> points(256);
    for (Size i = 0; i < points.size(); ++i) {
        const float temperature = float(range.lower()) + float(i) / (points.size() - 1) * float(range.size());
        points[i].value = float(i) / (points.size() - 1);

        if (temperature < draperPoint) {
            points[i].color = darkColor;
        } else {
            const Xyz xyz = getBlackBodyColor(temperature);
            const Rgba color = xyzToRgb(xyz);
            const Rgba normColor = color / max(color.r(), color.g(), color.b());
            const float weight = min((temperature - draperPoint) / (pureEmissionColor - draperPoint), 1.f);
            points[i].color = lerp(darkColor, normColor, weight);
        }
    }
    return Palette(std::move(points));
}

NAMESPACE_SPH_END
