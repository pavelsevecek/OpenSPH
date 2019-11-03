#pragma once

#include "gui/objects/Color.h"

NAMESPACE_SPH_BEGIN

/// Using http://filmicworlds.com/blog/filmic-tonemapping-with-piecewise-power-curves/
class FilmicMapping {
public:
    struct UserParams {
        float toeStrength = 0.f;
        float toeLength = 0.5f;
        float shoulderStrength = 0.f;
        float shoulderLength = 0.5f;
        float shoulderAngle = 0.f;
        float gamma = 1.f;
    };

private:
    struct DirectParams {
        float x0 = 0.25f;
        float y0 = 0.25f;
        float x1 = 0.75f;
        float y1 = 0.75f;
        float W = 1.f;
        float overshootX = 0.f;
        float overshootY = 0.f;
        float gamma = 1.f;
    };

    struct CurveSegment {
        float offsetX = 0.f;
        float offsetY = 0.f;
        float scaleX = 1.f;
        float scaleY = 1.f;
        float lnA = 0.f;
        float B = 1.f;

        float eval(const float x) const;
    };


    float W = 1.f;
    float invW = 1.f;

    float x0 = 0.25f;
    float x1 = 0.75f;
    float y0 = 0.25f;
    float y1 = 0.75f;

    StaticArray<CurveSegment, 3> m_segments;

public:
    void create(const UserParams& userParams);

    float operator()(const float x) const;

private:
    void create(const DirectParams& srcParams);
    void getDirectParams(DirectParams& dstParams, const UserParams& srcParams);
};


NAMESPACE_SPH_END
