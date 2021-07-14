#pragma once

/// \file Lut.h
/// \brief Approximation of generic function by look-up table
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/containers/Array.h"
#include "objects/wrappers/Interval.h"

NAMESPACE_SPH_BEGIN

template <typename TValue, typename TScalar = Float>
class Lut;

template <typename TValue, typename TScalar = Float>
class LutIterator {
private:
    Iterator<const TValue> iter;
    Size index;
    Size size;
    Interval range;

public:
    LutIterator(Iterator<const TValue> iter,
        const Size index,
        const Size size,
        const Interval& range,
        Badge<Lut<TValue, TScalar>>)
        : iter(iter)
        , index(index)
        , size(size)
        , range(range) {}

    LutIterator& operator++() {
        ++iter;
        ++index;
        return *this;
    }

    struct Value {
        TScalar x;
        TValue y;

        bool operator==(const Value& other) const {
            return x == other.x && y == other.y;
        }
    };

    Value operator*() const {
        return Value{ Float(index) / Float(size - 1) * range.size() + range.lower(), *iter };
    }

    bool operator!=(const LutIterator& other) const {
        return iter != other.iter;
    }
};

/// \brief Callable representing a generic R->T function, approximated using look-up table.
template <typename TValue, typename TScalar>
class Lut : public Noncopyable {
private:
    Array<TValue> data;
    Interval range;

public:
    Lut() = default;

    Lut(const Interval range, Array<TValue>&& data)
        : data(std::move(data))
        , range(range) {}

    template <typename TFunction>
    Lut(const Interval range, const Size resolution, TFunction&& func)
        : data(resolution)
        , range(range) {
        for (Size i = 0; i < resolution; ++i) {
            const Float x = range.lower() + Float(i) * range.size() / (resolution - 1);
            data[i] = func(x);
        }
    }

    INLINE TValue operator()(const TScalar x) const {
        const TScalar fidx = TScalar((x - range.lower()) / range.size() * (data.size() - 1));
        const int idx1 = int(floor(fidx));
        const int idx2 = idx1 + 1;
        if (idx2 >= int(data.size() - 1)) {
            /// \todo possibly make linear interpolation rather than just const value
            return data.back();
        } else if (idx1 <= 0) {
            return data.front();
        } else {
            const TScalar ratio = fidx - idx1;
            SPH_ASSERT(ratio >= TScalar(0) && ratio < TScalar(1));
            return lerp(data[idx1], data[idx2], ratio);
        }
    }

    /// \brief Returns an iterator accessing tabulated values.
    LutIterator<TValue, TScalar> begin() const {
        return LutIterator<TValue, TScalar>(data.begin(), 0, data.size(), range, {});
    }

    /// \brief Returns an iterator accessing tabulated values.
    LutIterator<TValue, TScalar> end() const {
        return LutIterator<TValue, TScalar>(data.end(), data.size(), data.size(), range, {});
    }

    /// \brief Returns the number of tabulated value.
    Size size() const {
        return data.size();
    }

    /// \brief Returns the definition interval of the function.
    Interval getRange() const {
        return range;
    }

    /// \brief Computes the derivative of the function.
    Lut derivative() const {
        Array<Float> deriv(data.size());
        const Float dx = range.size() / data.size();
        for (Size i = 0; i < data.size() - 1; ++i) {
            deriv[i] = (data[i + 1] - data[i]) / dx;
        }
        deriv[data.size() - 1] = deriv[data.size() - 2];
        return Lut(range, std::move(deriv));
    }

    /// \brief Computes the indefinite integral of the function.
    ///
    /// The integration constant is set so that the integral at x0 has value y0.
    Lut integral(const Float x0, const Float y0) const {
        SPH_ASSERT(range.contains(x0));
        Array<Float> integ(data.size());
        integ[0] = 0;
        const Float dx = range.size() / data.size();
        for (Size i = 1; i < data.size(); ++i) {
            integ[i] = integ[i - 1] + data[i] * dx;
        }
        const Lut lut(range, std::move(integ));
        return lut + (y0 - lut(x0));
    }
};

/// \brief Returns a LUT given by a generic binary operator.
///
/// The definition interval is given by intersection of the definition intervals of operands. If the
/// intersection is empty, returns an empty LUT.
template <typename TValue, typename TScalar, typename TBinaryOp>
Lut<TValue, TScalar> lutOperation(const Lut<TValue, TScalar>& lut1,
    const Lut<TValue, TScalar>& lut2,
    const TBinaryOp& op) {
    // use the higher resolution
    const Float dx = min(lut1.getRange().size() / lut1.size(), lut2.getRange().size() / lut2.size());

    const Interval range = lut1.getRange().intersect(lut2.getRange());
    const Size resolution = Size(round(range.size() / dx));

    auto func = [&lut1, &lut2, &op](const Float x) { return op(lut1(x), lut2(x)); };
    return Lut<TValue, TScalar>(range, resolution, func);
}

/// \brief Returns a LUT given by an operation between a given LUT and a scalar.
template <typename TValue, typename TScalar, typename TBinaryOp>
Lut<TValue, TScalar> lutOperation(const Lut<TValue, TScalar>& lut,
    const TScalar& value,
    const TBinaryOp& op) {
    auto func = [&lut, &value, &op](const Float x) { return op(lut(x), value); };
    return Lut<TValue, TScalar>(lut.getRange(), lut.size(), func);
}

template <typename TValue, typename TScalar>
Lut<TValue, TScalar> operator+(const Lut<TValue, TScalar>& lut1, const Lut<TValue, TScalar>& lut2) {
    return lutOperation(lut1, lut2, [](const Float x1, const Float x2) { return x1 + x2; });
}

template <typename TValue, typename TScalar>
Lut<TValue, TScalar> operator-(const Lut<TValue, TScalar>& lut1, const Lut<TValue, TScalar>& lut2) {
    return lutOperation(lut1, lut2, [](const Float x1, const Float x2) { return x1 - x2; });
}

template <typename TValue, typename TScalar>
Lut<TValue, TScalar> operator*(const Lut<TValue, TScalar>& lut1, const Lut<TValue, TScalar>& lut2) {
    return lutOperation(lut1, lut2, [](const Float x1, const Float x2) { return x1 * x2; });
}

template <typename TValue, typename TScalar>
Lut<TValue, TScalar> operator/(const Lut<TValue, TScalar>& lut1, const Lut<TValue, TScalar>& lut2) {
    return lutOperation(lut1, lut2, [](const Float x1, const Float x2) { return x1 / x2; });
}

template <typename TValue, typename TScalar>
Lut<TValue, TScalar> operator+(const Lut<TValue, TScalar>& lut1, const TScalar& mult) {
    return lutOperation(lut1, mult, [](const Float x1, const Float x2) { return x1 + x2; });
}

template <typename TValue, typename TScalar>
Lut<TValue, TScalar> operator-(const Lut<TValue, TScalar>& lut1, const TScalar& mult) {
    return lutOperation(lut1, mult, [](const Float x1, const Float x2) { return x1 - x2; });
}

template <typename TValue, typename TScalar>
Lut<TValue, TScalar> operator*(const Lut<TValue, TScalar>& lut1, const TScalar& mult) {
    return lutOperation(lut1, mult, [](const Float x1, const Float x2) { return x1 * x2; });
}

template <typename TValue, typename TScalar>
Lut<TValue, TScalar> operator/(const Lut<TValue, TScalar>& lut1, const TScalar& mult) {
    return lutOperation(lut1, mult, [](const Float x1, const Float x2) { return x1 / x2; });
}

NAMESPACE_SPH_END
