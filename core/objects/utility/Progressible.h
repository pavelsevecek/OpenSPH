#pragma once

/// \file Progressible.h
/// \brief Base class for objects that report the computation progress
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "math/MathBasic.h"
#include "objects/wrappers/Function.h"

NAMESPACE_SPH_BEGIN

/// \brief Class that reports the progress of computation.
///
/// It is designed as a base class. The derived classes should call \ref startProgress at the beginning of the
/// computation with the total number of 'steps' the computation takes. During the computation, classes should
/// periodically call \ref tickProgress; if the function returns false, it signals the computation should be
/// cancelled as soon as possible.
///
/// The callee may set a progress callback to receive the current status of the computation. The callback
/// takes the current relative progress (between 0 and 1), as well as any other information the class passes
/// to the \ref tickProgress call.
template <typename... TArgs>
class Progressible {
private:
    using ProgressFunc = Function<bool(const Float progress, const TArgs&... args)>;

    ProgressFunc callback;

    mutable Size step = 0;
    mutable Size target = 0;
    mutable Size next = 0;
    mutable std::atomic<Size> current{ 0 };

public:
    void setProgressCallback(const ProgressFunc& func) {
        callback = func;
    }

protected:
    void startProgress(const Size itemCnt) const {
        target = itemCnt;
        step = max(target / 100, 1u);
        current = 0;
        next = step;
    }

    bool tickProgress(const TArgs&... args) const {
        if (current++ == next) {
            next += step;

            if (callback) {
                return callback(Float(current) / target, args...);
            }
        }
        return true;
    }
};

NAMESPACE_SPH_END
