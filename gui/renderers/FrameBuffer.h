#pragma once

#include "gui/objects/Bitmap.h"
#include "gui/objects/Filmic.h"

NAMESPACE_SPH_BEGIN

class FrameBuffer : public Noncopyable {
private:
    FilmicMapping filmic;
    Bitmap<Rgba> values;
    Size passCnt = 0;

public:
    explicit FrameBuffer(const Pixel resolution) {
        values.resize(resolution, Rgba::transparent());

        FilmicMapping::UserParams params;
        params.toeStrength = 0.1f;
        params.toeLength = 0.1f;
        params.shoulderStrength = 2.f;
        params.shoulderLength = 0.4f;
        params.shoulderAngle = 0.f;
        filmic.create(params);
    }

    void accumulate(const Bitmap<Rgba>& pass) {
        SPH_ASSERT(pass.size() == values.size());
        for (int y = 0; y < values.size().y; ++y) {
            for (int x = 0; x < values.size().x; ++x) {
                Pixel p(x, y);
                const Rgba accumulatedColor = (pass[p] + values[p] * passCnt) / (passCnt + 1);
                values[p] = accumulatedColor.over(values[p]);
            }
        }
        passCnt++;
    }

    void override(Bitmap<Rgba>&& pass) {
        values = std::move(pass);
        passCnt = 1;
    }

    Bitmap<Rgba> getBitmap() const {
        Bitmap<Rgba> colormapped(values.size());
        for (int y = 0; y < values.size().y; ++y) {
            for (int x = 0; x < values.size().x; ++x) {
                const Rgba color = values[Pixel(x, y)];
                colormapped[Pixel(x, y)] =
                    Rgba(filmic(color.r()), filmic(color.g()), filmic(color.b()), color.a());
            }
        }
        return colormapped;
    }
};

NAMESPACE_SPH_END
