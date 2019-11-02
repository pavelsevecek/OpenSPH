#pragma once

#include "gui/objects/Color.h"

NAMESPACE_SPH_BEGIN

class FilmicMapping {
public:
    struct UserParams {
        float m_toeStrength = 0.f;
        float m_toeLength = 0.5f;
        float m_shoulderStrength = 0.f;
        float m_shoulderLength = 0.5f;
        float m_shoulderAngle = 0.f;

        float m_gamma = 1.f;
    };

    struct DirectParams {

        float m_x0 = 0.25f;
        float m_y0 = 0.25f;
        float m_x1 = 0.75f;
        float m_y1 = 0.75f;
        float m_W = 1.f;

        float m_overshootX = 0.f;
        float m_overshootY = 0.f;

        float m_gamma = 1.f;
    };

    struct CurveSegment {
        float eval(float x) const;

        float m_offsetX = 0.f;
        float m_offsetY = 0.f;
        float m_scaleX = 1.f;
        float m_scaleY = 1.f;
        float m_lnA = 0.f;
        float m_B = 1.f;
    };


    float m_W = 1.f;
    float m_invW = 1.f;

    float m_x0 = 0.25f;
    float m_x1 = 0.75f;
    float m_y0 = 0.25f;
    float m_y1 = 0.75f;

    StaticArray<CurveSegment, 3> m_segments;

    void create(const DirectParams& srcParams);
    void getDirectParams(DirectParams& dstParams, const UserParams& srcParams);

    float eval(const float x) const;
};


NAMESPACE_SPH_END
