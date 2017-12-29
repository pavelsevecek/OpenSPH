#pragma once

#include "objects/containers/Array.h"
#include "objects/containers/StaticArray.h"
#include "objects/wrappers/Function.h"
#include "objects/wrappers/Interval.h"
#include "post/Point.h"

NAMESPACE_SPH_BEGIN

namespace Post {
    Float correlationCoefficient(ArrayView<const PlotPoint> points);

    Float chiSquareDistribution(const Float chiSqr, const Float dof);

    Float chiSquareTest(ArrayView<const Float> measured, ArrayView<const Float> expected);

    Float kolmogorovSmirnovDistribution(const Float x);

    struct KsResult {
        Float D;
        Float prob;
    };

    /// One-dimensional Kolmogorov-Smirnov test with given CDF of expected probability distribution.
    KsResult kolmogorovSmirnovTest(ArrayView<const Float> data, const Function<Float(Float)>& expectedCdf);

    KsResult kolmogorovSmirnovTest(ArrayView<const Float> data1, ArrayView<const Float> data2);

    using KsFunction = Function<StaticArray<Float, 4>(PlotPoint)>;

    /// Two-dimensional Kolmogorov-Smirnov test
    KsResult kolmogorovSmirnovTest(ArrayView<const PlotPoint> data, const KsFunction& expected);

    KsFunction getUniformKsFunction(Interval rangeX, Interval rangeY);
} // namespace Post

NAMESPACE_SPH_END
