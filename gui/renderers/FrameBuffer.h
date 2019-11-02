#pragma once

#include "gui/objects/Bitmap.h"
#include "gui/objects/Filmic.h"

NAMESPACE_SPH_BEGIN

struct ShadeResult {
    Rgba color;
    BasicVector<float> velocity;
    float depth;
    float weight;

    ShadeResult() = default;

    ShadeResult(const Rgba& color,
        const BasicVector<float>& velocity = BasicVector<float>(0.f),
        const float depth = LARGE)
        : color(color)
        , velocity(velocity)
        , depth(depth) {
        weight = 1.f;
    }

    static ShadeResult black() {
        ShadeResult result;
        result.color = Rgba::transparent();
        result.velocity = BasicVector<float>(0.f);
        result.depth = 0.f;
        result.weight = 0.f;
        return result;
    }

    void accumulate(const ShadeResult& other) {
        const Rgba accumulatedColor = (color * weight + other.color * other.weight) / (weight + other.weight);
        color = accumulatedColor.over(color);

        velocity = (velocity * weight + other.velocity * other.weight) / (weight + other.weight);
        depth = (depth * weight + other.depth * other.weight) / (weight + other.weight);

        weight += other.weight;
    }
};

class FrameBuffer : public Noncopyable {
private:
    Bitmap<ShadeResult> values;

public:
    explicit FrameBuffer(const Pixel resolution) {
        values.resize(resolution, ShadeResult::black());
    }

    void accumulate(const Bitmap<ShadeResult>& pass) {
        ASSERT(pass.size() == values.size());
        for (int y = 0; y < values.size().y; ++y) {
            for (int x = 0; x < values.size().x; ++x) {
                Pixel p(x, y);
                values[p].accumulate(pass[p]);
            }
        }
    }

    void override(Bitmap<ShadeResult>&& pass) {
        values = std::move(pass);
    }

    Bitmap<Rgba> motionBlur() const {
        return {};
    }

    /*const Bitmap<Rgba>& bitmap() const {
        return values;
    }*/
};

NAMESPACE_SPH_END
