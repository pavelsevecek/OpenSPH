#pragma once

/// \file Stats.h
/// \brief Benchmark statistics
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "bench/Common.h"
#include "objects/wrappers/Range.h"

NAMESPACE_BENCHMARK_BEGIN

class Stats {
private:
    Float sum = 0._f;
    Float sumSqr = 0._f;
    Size cnt = 0;
    Range range;

public:
    INLINE void add(const Float value) {
        sum += value;
        sumSqr += sqr(value);
        range.extend(value);
        cnt++;
    }

    INLINE Float mean() const {
        ASSERT(cnt != 0);
        return sum / cnt;
    }

    INLINE Float variance() const {
        if (cnt < 2) {
            return INFTY;
        }
        const Float cntInv = 1._f / cnt;
        return cntInv * (sumSqr * cntInv - sqr(sum * cntInv));
    }

    INLINE Size count() const {
        return cnt;
    }

    INLINE Float min() const {
        return range.lower();
    }

    INLINE Float max() const {
        return range.upper();
    }
};

NAMESPACE_BENCHMARK_END
