#include "gui/ImageTransform.h"

NAMESPACE_SPH_BEGIN


Rgba interpolate(const Bitmap<Rgba>& bitmap, const float x, const float y) {
    const Pixel size = bitmap.size();
    const Vector uvw =
        clamp(Vector(x, y, 0._f), Vector(0._f), Vector(size.x - 1, size.y - 1, 0._f) - Vector(EPS));
    const Size u1 = int(uvw[X]);
    const Size v1 = int(uvw[Y]);
    const Size u2 = u1 + 1;
    const Size v2 = v1 + 1;
    const float a = float(uvw[X] - u1);
    const float b = float(uvw[Y] - v1);
    SPH_ASSERT(a >= 0.f && a <= 1.f, a);
    SPH_ASSERT(b >= 0.f && b <= 1.f, b);

    return Rgba(bitmap[Pixel(u1, v1)]) * (1.f - a) * (1.f - b) +
           Rgba(bitmap[Pixel(u2, v1)]) * a * (1.f - b) + //
           Rgba(bitmap[Pixel(u1, v2)]) * (1.f - a) * b + //
           Rgba(bitmap[Pixel(u2, v2)]) * a * b;
}

Bitmap<Rgba> resize(const Bitmap<Rgba>& input, const Pixel size) {
    const float scaleX = float(input.size().x) / size.x;
    const float scaleY = float(input.size().y) / size.y;
    if (min(scaleX, scaleY) > 2) {
        // first do area-based scaling to 1/2 (and possibly recursively more)
        Bitmap<Rgba> half(input.size() / 2);
        for (int y = 0; y < half.size().y; ++y) {
            for (int x = 0; x < half.size().x; ++x) {
                const int x1 = 2 * x;
                const int y1 = 2 * y;
                half(x, y) =
                    (input(x1, y1) + input(x1 + 1, y1) + input(x1, y1 + 1) + input(x1 + 1, y1 + 1)) / 4.f;
            }
        }
        return resize(half, size);
    } else {
        Bitmap<Rgba> resized(size);
        for (int y = 0; y < size.y; ++y) {
            for (int x = 0; x < size.x; ++x) {
                resized(x, y) = interpolate(input, scaleX * x, scaleY * y);
            }
        }
        return resized;
    }
}

Bitmap<float> detectEdges(const Bitmap<Rgba>& input) {
    Bitmap<float> disc(input.size());
    Rectangle rect(Pixel(0, 0), input.size() - Pixel(1, 1));
    for (int y = 0; y < input.size().y; ++y) {
        for (int x = 0; x < input.size().x; ++x) {
            Rectangle patch = rect.intersect(Rectangle::window(Pixel(x, y), 1));
            const float intensity2 = input(x, y).intensity();
            float maxDiff = 0.f;
            for (int y1 : patch.rowRange()) {
                for (int x1 : patch.colRange()) {
                    const float intensity1 = input(x1, y1).intensity();
                    maxDiff = max(maxDiff, sqr(intensity1 - intensity2));
                }
            }
            disc(x, y) = maxDiff;
        }
    }
    return disc;
}

template <typename TFilter>
Bitmap<Rgba> filter(IScheduler& scheduler,
    const Bitmap<Rgba>& input,
    const Size radius,
    const TFilter& func) {
    Bitmap<Rgba> result(input.size());
    Rectangle rect(Pixel(0, 0), input.size() - Pixel(1, 1));
    parallelFor(scheduler, 0, input.size().y, 1, [&](const Size y) {
        for (int x = 0; x < input.size().x; ++x) {
            Rgba sum = Rgba::black();
            float weight = 0.f;
            Rectangle window = rect.intersect(Rectangle::window(Pixel(x, y), radius));
            for (int y1 : window.rowRange()) {
                for (int x1 : window.colRange()) {
                    const float w = func(Pixel(x, y), Pixel(x1, y1));
                    SPH_ASSERT(isReal(w));
                    sum += input(x1, y1) * w;
                    weight += w;
                }
            }
            if (weight > 0) {
                SPH_ASSERT(isReal(sum.r()) && isReal(sum.g()) && isReal(sum.b()));
                SPH_ASSERT(weight > 0.f, weight);
                result(x, y) = sum / weight;
            } else {
                result(x, y) = Rgba::black();
            }
        }
    });
    return result;
}

Bitmap<Rgba> gaussianBlur(IScheduler& scheduler, const Bitmap<Rgba>& input, const Size radius) {
    const float sigma = radius / 2.f;
    const float norm = 1.f / (2.f * sqr(sigma));
    return filter(scheduler, input, radius, [norm](const Pixel& p1, const Pixel& p2) {
        const float dist = getLength(p1 - p2);
        return exp(-sqr(dist) * norm);
    });
}

Bitmap<Rgba> denoise(IScheduler& scheduler, const Bitmap<Rgba>& input, const DenoiserParams& params) {
    Rectangle rect(Pixel(0, 0), input.size() - Pixel(1, 1));
    const float norm = 1.f / (2.f * sqr(params.sigma));
    return filter(scheduler, input, params.filterRadius, [&](const Pixel& p1, const Pixel& p2) {
        Rectangle patch = rect.intersect(Rectangle::window(p2, params.patchRadius));
        float distSqr = 0.f;
        int count = 0;
        for (int y : patch.rowRange()) {
            for (int x : patch.colRange()) {
                const Pixel pp2(x, y);
                const Pixel pp1(pp2 - p2 + p1);
                if (!rect.contains(pp1)) {
                    continue;
                }
                SPH_ASSERT(rect.contains(pp2));

                const Rgba v1 = input[pp1];
                const Rgba v2 = input[pp2];
                distSqr += sqr(v1.r() - v2.r()) + sqr(v1.g() - v2.g()) + sqr(v1.b() - v2.b());
                count++;
            }
        }
        SPH_ASSERT(count > 0);
        SPH_ASSERT(distSqr < LARGE, distSqr);
        distSqr /= 3 * count;
        return exp(-min(distSqr * norm, 8.f));
    });
}

constexpr float DISCONTINUITY_WEIGHT = 1.e-3f;

Bitmap<Rgba> denoiseLowFrequency(IScheduler& scheduler,
    const Bitmap<Rgba>& input,
    const DenoiserParams& params,
    const Size levels) {
    Bitmap<Rgba> small = resize(input, input.size() / 2);
    Bitmap<Rgba> denoised = denoise(scheduler, small, params);
    if (levels > 1) {
        DenoiserParams levelParams = params;
        levelParams.sigma *= 0.5f;
        denoised = denoiseLowFrequency(scheduler, denoised, levelParams, levels - 1);
    }
    Bitmap<Rgba> smallUpscaled = resize(small, input.size());
    Bitmap<Rgba> denoisedUpscaled = resize(denoised, input.size());

    // add high-frequency details
    Bitmap<float> disc = detectEdges(smallUpscaled);
    constexpr float norm = 1.f / DISCONTINUITY_WEIGHT;
    for (int y = 0; y < input.size().y; ++y) {
        for (int x = 0; x < input.size().x; ++x) {
            const Pixel p(x, y);
            const Rgba c_0 = input[p];
            const Rgba c_f = denoisedUpscaled[p] + (input[p] - smallUpscaled[p]);
            const float w = exp(-norm * disc[p]);
            SPH_ASSERT(isReal(c_0) && isReal(c_f) && isReal(w));
            denoisedUpscaled[p] = lerp(c_0, c_f, w);
        }
    }
    return denoisedUpscaled;
}

NAMESPACE_SPH_END