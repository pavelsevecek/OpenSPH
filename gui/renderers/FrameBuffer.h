#pragma once

#include "gui/objects/Bitmap.h"
#include "gui/objects/Filmic.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

class IColorMap : public Polymorphic {
public:
    virtual void map(IScheduler& scheduler, Bitmap<Rgba>& values) const = 0;
};

class LogarithmicColorMap : public IColorMap {
private:
    float factor = 2.f;
    float saturation = 0.5f;

public:
    explicit LogarithmicColorMap(const float factor)
        : factor(factor) {}

    virtual void map(IScheduler& scheduler, Bitmap<Rgba>& values) const override {
        parallelFor(scheduler, 0, values.size().y, 1, [this, &values](const int y) {
            for (int x = 0; x < values.size().x; ++x) {
                Rgba& color = values[Pixel(x, y)];
                float oldIntensity = color.intensity();
                float newIntensity = map(oldIntensity);
                const Rgba saturatedColor = color * newIntensity / oldIntensity;
                const Rgba desaturatedColor = Rgba(map(color.r()), map(color.g()), map(color.b()), color.a());
                color = lerp(desaturatedColor, saturatedColor, saturation);
            }
        });
    }

    void setFactor(const float newFactor) {
        factor = newFactor;
    }

private:
    inline float map(const float x) const {
        return 1.f / factor * log(1.f + factor * x);
    }
};

class FilmicColorMap : public IColorMap {
private:
    FilmicMapping filmic;

public:
    FilmicColorMap() {
        FilmicMapping::UserParams params;
        params.toeStrength = 0.1f;
        params.toeLength = 0.1f;
        params.shoulderStrength = 2.f;
        params.shoulderLength = 0.4f;
        params.shoulderAngle = 0.f;
        filmic.create(params);
    }

    virtual void map(IScheduler& scheduler, Bitmap<Rgba>& values) const override {
        parallelFor(scheduler, 0, values.size().y, 1, [this, &values](const int y) {
            for (int x = 0; x < values.size().x; ++x) {
                Rgba& color = values[Pixel(x, y)];
                color = Rgba(filmic(color.r()), filmic(color.g()), filmic(color.b()), color.a());
            }
        });
    }
};

class FrameBuffer : public Noncopyable {
private:
    Bitmap<Rgba> values;
    Size passCnt = 0;

public:
    FrameBuffer(const Pixel resolution) {
        values.resize(resolution, Rgba::transparent());
    }

    void accumulate(IScheduler& scheduler, const Bitmap<Rgba>& pass) {
        SPH_ASSERT(pass.size() == values.size());
        parallelFor(scheduler, 0, values.size().y, 1, [this, &pass](const int y) {
            for (int x = 0; x < values.size().x; ++x) {
                Pixel p(x, y);
                const Rgba accumulatedColor = (pass[p] + values[p] * passCnt) / (passCnt + 1);
                values[p] = accumulatedColor.over(values[p]);
            }
        });
        passCnt++;
    }

    void override(Bitmap<Rgba>&& pass) {
        values = std::move(pass);
        passCnt = 1;
    }

    const Bitmap<Rgba>& getBitmap() const& {
        return values;
    }

    Bitmap<Rgba> getBitmap() && {
        return std::move(values);
    }
};

NAMESPACE_SPH_END
