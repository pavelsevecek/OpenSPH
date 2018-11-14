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

    explicit operator wxColour() const {
        return wxColour(getByte(data[0]), getByte(data[1]), getByte(data[2]));
    }

    float operator[](const Size idx) const {
        return data[idx];
    }

    float& operator[](const Size idx) {
        return data[idx];
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
        return (data[0] + data[1] + data[2]) / 3.f;
    }

    float alpha() const {
        return data[3];
    }

    /// \brief Blends two colors together using "over" operation.
    Rgba over(const Rgba& other) const {
        const float a1 = other.alpha();
        const float a2 = this->alpha();
        const float ar = a2 + a1 * (1.f - a2);
        ASSERT(ar > 0.f);
        Rgba color = (data * a2 + other.data * a1 * (1.f - a2)) / ar;
        color[3] = ar;
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

    /*INLINE friend Rgba max(const Rgba& c1, const Rgba& c2) {
        return Rgba(max(c1[0], c2[0]), max(c1[1], c2[1]), max(c1[2], c2[2]));
    }*/

private:
    Rgba preserveAlpha(const Rgba& color) const {
        Rgba result = color;
        result[3] = data[3];
        return result;
    }

    int getByte(const float f) const {
        return clamp(int(f * 255.f), 0, 255);
    }
};


NAMESPACE_SPH_END
