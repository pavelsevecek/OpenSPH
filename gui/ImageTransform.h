#pragma once

#include "gui/objects/Bitmap.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

template <typename Transform>
Bitmap<Rgba> transform(const Bitmap<Rgba>& input, const Transform& func) {
    Bitmap<Rgba> result(input.size());
    for (Size y = 0; y < input.size().y; ++y) {
        for (Size x = 0; x < input.size().x; ++x) {
            result(x, y) = func(Pixel(x, y), input(x, y));
        }
    }
    return result;
}

Rgba interpolate(const Bitmap<Rgba>& bitmap, const float x, const float y);

Bitmap<Rgba> resize(const Bitmap<Rgba>& input, const Pixel size);

Bitmap<float> detectEdges(const Bitmap<Rgba>& input);

Bitmap<Rgba> gaussianBlur(IScheduler& scheduler, const Bitmap<Rgba>& input, const Size radius);

struct DenoiserParams {
    Size filterRadius = 5;
    Size patchRadius = 2;
    float sigma = 0.02f;
};

Bitmap<Rgba> denoise(IScheduler& scheduler, const Bitmap<Rgba>& input, const DenoiserParams& params);

Bitmap<Rgba> denoiseLowFrequency(IScheduler& scheduler,
    const Bitmap<Rgba>& input,
    const DenoiserParams& params,
    const Size levels = 2);

NAMESPACE_SPH_END
