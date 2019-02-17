#include "post/StatisticTests.h"
#include <algorithm>
#include <numeric>

NAMESPACE_SPH_BEGIN

Float Post::correlationCoefficient(ArrayView<const PlotPoint> points) {
    ASSERT(points.size() >= 2);
    // find the mean
    PlotPoint mean(0._f, 0._f);
    for (PlotPoint p : points) {
        mean += p;
    }
    mean /= points.size();

    Float corr = 0._f;
    Float normX = 0._f;
    Float normY = 0._f;
    for (PlotPoint p : points) {
        corr += (p.x - mean.x) * (p.y - mean.y);
        normX += sqr(p.x - mean.x);
        normY += sqr(p.y - mean.y);
    }
    // may be slightly over/below 1/-1 due to round-off errors
    return corr / (sqrt(normX * normY));
}

Float Post::chiSquareDistribution(const Float chiSqr, const Float dof) {
    return 1._f / (pow(2._f, 0.5_f * dof) * std::tgamma(0.5_f * dof)) * pow(chiSqr, 0.5_f * dof - 1._f) *
           exp(-0.5_f * chiSqr);
}

Float Post::chiSquareTest(ArrayView<const Float> measured, ArrayView<const Float> expected) {
    ASSERT(measured.size() == expected.size());
    Float chiSqr = 0._f;
    for (Size i = 0; i < measured.size(); ++i) {
        ASSERT(measured[i] >= 0._f && expected[i] >= 0._f);
        if (expected[i] == 0._f) {
            if (measured[i] == 0._f) {
                continue;
            } else {
                // measured nonzero, but expected is zero -> measured cannot be from the expected
                // distribution
                return INFINITY;
            }
        }
        chiSqr += sqr(measured[i] - expected[i]) / expected[i];
    }
    return chiSqr;
}

// Numerical recipes 624
Float Post::kolmogorovSmirnovDistribution(const Float x) {
    constexpr Float eps1 = 1.e-3_f;
    constexpr Float eps2 = 1.e-8_f;

    Float Q = 0._f;
    Float prevTerm = 0._f;
    for (Size j = 1; j < 100; ++j) {
        const Float term = (int(isOdd(j)) * 2 - 1) * exp(-2._f * sqr(j) * sqr(x));
        Q += term;
        if (abs(term) <= eps1 * prevTerm || abs(term) <= eps2 * Q) {
            return 2._f * Q;
        }
        prevTerm = abs(term);
    }
    return 1._f;
}

static Array<Float> sortData(ArrayView<const Float> data) {
    Array<Float> sortedData;
    sortedData.pushAll(data.begin(), data.end());
    std::sort(sortedData.begin(), sortedData.end(), [](Float p1, Float p2) { return p1 < p2; });
    return sortedData;
}

static Array<PlotPoint> makeCdf(ArrayView<const Float> pdf) {
    Array<Float> sortedPdf = sortData(pdf);
    Array<PlotPoint> cdf(pdf.size());
    const Float step = 1._f / (pdf.size() - 1);
    for (Size i = 0; i < pdf.size(); ++i) {
        cdf[i] = { sortedPdf[i], i * step };
    }
    ASSERT(cdf.front().y == 0 && cdf.back().y == 1);
    return cdf;
}

static Float ksProb(const Float sqrtN, const Float D) {
    return Post::kolmogorovSmirnovDistribution((sqrtN + 0.12_f + 0.11_f / sqrtN) * D);
}

Post::KsResult Post::kolmogorovSmirnovTest(ArrayView<const Float> data,
    const Function<Float(Float)>& expectedCdf) {
    ASSERT(data.size() >= 2);
    Array<PlotPoint> cdf = makeCdf(data);

    // find the maximum difference (Kolmogorov-Smirnov D)
    Float D = 0._f;
    Float prevY = 0._f;
    for (PlotPoint p : cdf) {
        const Float expectedY = expectedCdf(p.x);
        D = max(D, abs(p.y - expectedY), abs(prevY - expectedY));
        prevY = p.y;
    }
    const Float sqrtN = sqrt(data.size());
    Float prob = ksProb(sqrtN, D);
    ASSERT(prob >= 0._f && prob <= 1._f);
    return { D, prob };
}

Post::KsResult Post::kolmogorovSmirnovTest(ArrayView<const Float> data1, ArrayView<const Float> data2) {
    Array<PlotPoint> cdf1 = makeCdf(data1);
    Array<PlotPoint> cdf2 = makeCdf(data2);

    Float D = 0._f;
    for (Size i = 0, j = 0; i < cdf1.size() && j < cdf2.size();) {
        if (cdf1[i].x <= cdf2[j].x) {
            ++i;
        }
        if (cdf1[i].x >= cdf2[j].x) {
            ++j;
        }
        D = max(D, abs(cdf1[i].y - cdf2[j].y));
    }

    const Float sqrtNe = sqrt((data1.size() * data2.size()) / (data1.size() + data2.size()));
    Float prob = ksProb(sqrtNe, D);
    ASSERT(prob >= 0._f && prob <= 1._f);
    return { D, prob };
}


static StaticArray<Float, 4> countQuadrants(const PlotPoint origin, ArrayView<const PlotPoint> data) {
    StaticArray<Float, 4> quadrants;
    quadrants.fill(0._f);
    for (PlotPoint p : data) {
        if (p.y > origin.y) {
            p.x > origin.x ? quadrants[0]++ : quadrants[1]++;
        } else {
            p.x > origin.x ? quadrants[3]++ : quadrants[2]++;
        }
    }
    for (Float& q : quadrants) {
        q /= data.size();
    }
    return quadrants;
}

Post::KsResult Post::kolmogorovSmirnovTest(ArrayView<const PlotPoint> data, const KsFunction& expected) {
    Float D = 0._f;
    for (PlotPoint p : data) {
        StaticArray<Float, 4> measuredQuadrants = countQuadrants(p, data);
        StaticArray<Float, 4> expectedQuadrants = expected(p);
        for (Size i = 0; i < 4; ++i) {
            D = max(D, measuredQuadrants[i] - expectedQuadrants[i]);
        }
    }

    const Float sqrtNe = sqrt(data.size());
    const Float r = correlationCoefficient(data);
    const Float prob =
        kolmogorovSmirnovDistribution(sqrtNe * D / (1._f + sqrt(1._f - sqr(r)) * (0.25_f - 0.75_f / sqrtNe)));
    ASSERT(prob >= 0._f && prob <= 1._f);
    return { D, prob };
}

Post::KsFunction Post::getUniformKsFunction(const Interval rangeX, const Interval rangeY) {
    return [rangeX, rangeY](PlotPoint p) -> StaticArray<Float, 4> {
        const Float x = clamp((p.x - rangeX.lower()) / rangeX.size(), 0._f, 1._f);
        const Float y = clamp((p.y - rangeY.lower()) / rangeY.size(), 0._f, 1._f);
        return { (1._f - x) * (1._f - y), x * (1._f - y), x * y, (1._f - x) * y };
    };
}

NAMESPACE_SPH_END
