#pragma once

#include "geometry/Vector.h"

#include <wx/colour.h>

NAMESPACE_SPH_BEGIN

class Color : public Object {
private:
    Vector data;

    Color(const Vector& data)
        : data(data) {}

public:
    Color() = default;

    Color(const float r, const float g, const float b)
        : data(r, g, b) {}

    Color(const Color& other)
        : data(other.data) {}

    Color(const wxColour& other)
        : data(other.Red() / 255.f, other.Green() / 255.f, other.Blue() / 255.f) {}

    operator wxColour() const {
        return wxColour(int(data[0] * 255.f), int(data[1] * 255.f), int(data[2] * 255.f));
    }

    Color operator*(const float value) const { return data * value; }

    Color operator*(const Color& other) const { return data * other.data; }

    Color operator+(const Color& other) const { return data + other.data; }

    static Color red() { return Color(1.f, 0.f, 0.f); }

    static Color green() { return Color(0.f, 1.f, 0.f); }

    static Color blue() { return Color(0.f, 0.f, 1.f); }

    static Color black() { return Color(0.f, 0.f, 0.f); }

    static Color white() { return Color(1.f, 1.f, 1.f); }

    static Color gray(const float value = 0.5f) { return Color(value, value, value); }
};

NAMESPACE_SPH_END
