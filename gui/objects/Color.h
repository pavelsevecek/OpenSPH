#pragma once

#include "objects/geometry/Vector.h"
#include <wx/colour.h>

NAMESPACE_SPH_BEGIN

class Rgba {
private:
    BasicVector<float> data;

    Rgba(const BasicVector<float>& data)
        : data(data) {}

public:
    Rgba() = default;

    Rgba(const float r, const float g, const float b, const float a = 1.f)
        : data(r, g, b, a) {}

    explicit Rgba(const float gray, const float a = 1.f)
        : data(gray, gray, gray, a) {}

    Rgba(const Rgba& other)
        : data(other.data) {}

    explicit Rgba(const wxColour& other)
        : data(other.Red() / 255.f, other.Green() / 255.f, other.Blue() / 255.f, 1.f) {}

    Rgba& operator=(const Rgba& other) {
        data = other.data;
        return *this;
    }

    explicit operator wxColour() const {
        return wxColour(getByte(data[0]), getByte(data[1]), getByte(data[2]));
    }

    float& r() {
        return data[0];
    }

    float r() const {
        return data[0];
    }

    float& g() {
        return data[1];
    }

    float g() const {
        return data[1];
    }

    float& b() {
        return data[2];
    }

    float b() const {
        return data[2];
    }

    float& a() {
        return data[3];
    }

    float a() const {
        return data[3];
    }

    /// \brief Multiplies the intensity of the color by given factor.
    ///
    /// The alpha value is not affected by this operation.
    Rgba operator*(const float value) const {
        return preserveAlpha(data * value);
    }

    /// \brief Divides the intensity of the color by given factor.
    ///
    /// The alpha value is not affected by this operation.
    Rgba operator/(const float value) const {
        return preserveAlpha(data / value);
    }

    /// \brief Multiplies the color by other color, component-wise.
    ///
    /// \warning The resulting color has the alpha value of the left-hand side color, i.e. this operation is
    /// not commutative!
    Rgba operator*(const Rgba& other) const {
        return preserveAlpha(data * other.data);
    }

    /// \brief Returns the sum of two colors, component-wise.
    ///
    /// \warning The resulting color has the alpha value of the left-hand side color, i.e. this operation is
    /// not commutative!
    Rgba operator+(const Rgba& other) const {
        return preserveAlpha(data + other.data);
    }

    Rgba& operator+=(const Rgba& other) {
        *this = *this + other;
        return *this;
    }

    /// \brief Returns the average intensity of the color.
    float intensity() const {
        return 0.2126f * data[0] + 0.7152f * data[1] + 0.0722f * data[2];
    }

    /// \brief Blends two colors together using "over" operation.
    Rgba over(const Rgba& other) const {
        const float a1 = other.a();
        const float a2 = this->a();
        const float ar = a2 + a1 * (1.f - a2);
        ASSERT(ar > 0.f);
        Rgba color = (data * a2 + other.data * a1 * (1.f - a2)) / ar;
        color.a() = ar;
        return color;
    }

    /// \brief Returns a color darker by given factor.
    ///
    /// \param amount Value in interval [0, 1], where 0 = current color, 1 = black.
    Rgba darken(const float amount) const {
        ASSERT(amount >= 0.f && amount <= 1.f);
        return preserveAlpha(*this * (1.f - amount));
    }

    /// \brief Returns a color brighter by given factor.
    ///
    /// \param amount Value in interval [0, INFTY], where 0 = current color, 1 = 100% more brighter, etc.
    Rgba brighten(const float amount) const {
        ASSERT(amount >= 0.f);
        return preserveAlpha(*this * (1.f + amount));
    }

    /// \brief Returns an inverse color.
    Rgba inverse() const {
        return preserveAlpha(max(BasicVector<float>(1.f) - data, BasicVector<float>(0.f)));
    }

    /// \brief Computes a linear interpolation of two color.
    ///
    /// For amount 0, function returns this color, for amount 1 it returns the other color.
    Rgba blend(const Rgba& other, const float amount) const {
        return Rgba(lerp(data, other.data, amount));
    }

    static Rgba red() {
        return Rgba(1.f, 0.f, 0.f);
    }

    static Rgba green() {
        return Rgba(0.f, 1.f, 0.f);
    }

    static Rgba blue() {
        return Rgba(0.f, 0.f, 1.f);
    }

    static Rgba black() {
        return Rgba(0.f, 0.f, 0.f);
    }

    static Rgba white() {
        return Rgba(1.f, 1.f, 1.f);
    }

    static Rgba gray(const float value = 0.5f) {
        return Rgba(value, value, value);
    }

    static Rgba transparent() {
        return Rgba(0.f, 0.f, 0.f, 0.f);
    }

private:
    Rgba preserveAlpha(const Rgba& color) const {
        Rgba result = color;
        result.a() = data[3];
        return result;
    }

    int getByte(const float f) const {
        return clamp(int(f * 255.f), 0, 255);
    }
};

class Hsv {
private:
    BasicVector<float> data;

public:
    Hsv(const float h, const float s, const float v)
        : data(h, s, v) {}

    float& operator[](const int index) {
        ASSERT(index < 3);
        return data[index];
    }

    float& h() {
        return data[0];
    }

    float h() const {
        return data[0];
    }

    float& s() {
        return data[1];
    }

    float s() const {
        return data[1];
    }

    float& v() {
        return data[2];
    }

    float v() const {
        return data[2];
    }
};
/*
template <>
inline Hsv convert(const Rgba& rgb) {
    const float cmin = min(rgb.r(), rgb.g(), rgb.b());
    const float cmax = max(rgb.r(), rgb.g(), rgb.b());
    const float delta = cmax - cmin;
    if (delta < 1.e-3f) {
        return Hsv(0.f, 0.f, cmax);
    }
    const float v = cmax;
    const float s = (cmax < 1.e-3f) ? 0.f : delta / cmax;
    float h;
    if (cmax == rgb.r()) {
        h = (rgb.g() - rgb.b()) / delta;
    } else if (cmax == rgb.g()) {
        h = 2.f + (rgb.b() - rgb.r()) / delta;
    } else {
        h = 4.f + (rgb.r() - rgb.g()) / delta;
    }
    if (h >= 0.f) {
        h /= 6.f;
    } else {
        h = h / 6.f + 1.f;
    }
    return Hsv(h, s, v);
}

template <>
inline Rgba convert(const Hsv& hsv) {
    if (hsv.s() == 0.f) {
        return Rgba(hsv.v());
    }
    const float h = hsv.h() * 6.f;
    const int hidx = int(h);
    const float diff = h - hidx;
    const float p = hsv.v() * (1.f - hsv.s());
    const float q = hsv.v() * (1.f - (hsv.s() * diff));
    const float t = hsv.v() * (1.f - (hsv.s() * (1.f - diff)));

    switch (hidx) {
    case 0:
        return Rgba(hsv.v(), t, p);
    case 1:
        return Rgba(q, hsv.v(), p);
    case 2:
        return Rgba(p, hsv.v(), t);
    case 3:
        return Rgba(p, q, hsv.v());
    case 4:
        return Rgba(t, p, hsv.v());
    default:
        return Rgba(hsv.v(), p, q);
    }
}
*/

NAMESPACE_SPH_END
