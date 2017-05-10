#pragma once

/// \file Plot.h
/// \brief Drawing quantity values as functions of time or spatial coordinates
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

struct PlotPoint {
    Float x, y;
};

namespace Abstract {
    class Plot : public Polymorphic {
    public:
        virtual Range getRange(const Size coord) const = 0;

        virtual Array<PlotPoint> plot() const = 0;
    };
}

class TestPlot : public Abstract::Plot {
public:
    virtual Range getRange(const Size coord) const override {
        switch (coord) {
        case X:
            return Range(0._f, 2 * PI);
        case Y:
            return Range(-1._f, 1._f);
        default:
            STOP;
        }
    }

    virtual Array<PlotPoint> plot() const override {
        Array<PlotPoint> points;
        for (Float x = 0; x < 2._f * PI; x += 0.1_f) {
            points.push(PlotPoint{ x, sin(x) });
        }
        return points;
    }
};

class WeightedPlot : public Polymorphic {
protected:
    Array<Float> values;
    Array<Float> weights;
    Range range;

public:
    // virtual void setData(const Storage& storage) {}

    virtual void addPoint(const Float x, const Float y, const Float w) {
        if (!range.contains(x)) {
            return;
        }
        const Size idx = (x - range.lower()) / range.size() * (values.size() - 1);
        values[idx] = (values[idx] * weights[idx] + y * w) / (weights[idx] + w);
        weights[idx] += w;
    }

    template <typename TFunctor>
    void plot(TFunctor&& functor) {
        for (Size i = 0; i < values.size(); ++i) {
            const Float x = range.lower() + (range.size() * i) / (values.size() - 1);
            functor(x, values[i]);
        }
    }
};


NAMESPACE_SPH_END
