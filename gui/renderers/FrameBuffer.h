#pragma once

#include "gui/objects/Bitmap.h"

NAMESPACE_SPH_BEGIN

class FrameBuffer : public Noncopyable {
private:
    Bitmap<Rgba> values;
    Size passCnt = 0;

public:
    explicit FrameBuffer(const Pixel resolution) {
        values.resize(resolution, Rgba::transparent());
    }

    void accumulate(const Bitmap<Rgba>& pass) {
        ASSERT(pass.size() == values.size());
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

    const Bitmap<Rgba>& bitmap() const {
        return values;
    }
};

NAMESPACE_SPH_END
