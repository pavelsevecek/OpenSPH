#include "gui/objects/Filmic.h"

NAMESPACE_SPH_BEGIN


float FilmicMapping::CurveSegment::eval(const float x) const {
    float x0 = (x - offsetX) * scaleX;
    float y0 = 0.0f;

    // log(0) is undefined but our function should evaluate to 0. There are better ways to handle this,
    // but it's doing it the slow way here for clarity.
    if (x0 > 0) {
        y0 = expf(lnA + B * logf(x0));
    }

    return y0 * scaleY + offsetY;
}

void FilmicMapping::create(const UserParams& userParams) {
    DirectParams directParams;
    this->getDirectParams(directParams, userParams);
    this->create(directParams);
}

float FilmicMapping::operator()(const float x) const {
    const float normX = x * invW;
    int index = (normX < x0) ? 0 : ((normX < x1) ? 1 : 2);
    CurveSegment segment = m_segments[index];
    return segment.eval(normX);
}

// find a function of the form:
//   f(x) = e^(lnA + Bln(x))
// where
//   f(0)   = 0; not really a constraint
//   f(x0)  = y0
//   f'(x0) = m
static void SolveAB(float& lnA, float& B, float x0, float y0, float m) {
    B = (m * x0) / y0;
    lnA = logf(y0) - B * logf(x0);
}

// convert to y=mx+b
void AsSlopeIntercept(float& m, float& b, float x0, float x1, float y0, float y1) {
    float dy = (y1 - y0);
    float dx = (x1 - x0);
    if (dx == 0)
        m = 1.0f;
    else
        m = dy / dx;

    b = y0 - x0 * m;
}

// f(x) = (mx+b)^g
// f'(x) = gm(mx+b)^(g-1)
float EvalDerivativeLinearGamma(float m, float b, float g, float x) {
    float ret = g * m * powf(m * x + b, g - 1.0f);
    return ret;
}

void FilmicMapping::create(const DirectParams& srcParams) {
    DirectParams params = srcParams;

    // dstCurve.Reset();
    W = srcParams.W;
    invW = 1.0f / srcParams.W;

    // normalize params to 1.0 range
    params.W = 1.0f;
    params.x0 /= srcParams.W;
    params.x1 /= srcParams.W;
    params.overshootX = srcParams.overshootX / srcParams.W;

    float toeM = 0.0f;
    float shoulderM = 0.0f;
    {
        float m, b;
        AsSlopeIntercept(m, b, params.x0, params.x1, params.y0, params.y1);

        float g = srcParams.gamma;

        // base function of linear section plus gamma is
        // y = (mx+b)^g

        // which we can rewrite as
        // y = exp(g*ln(m) + g*ln(x+b/m))

        // and our evaluation function is (skipping the if parts):
        /*
                float x0 = (x - m_offsetX)*m_scaleX;
                y0 = expf(m_lnA + m_B*logf(x0));
                return y0*m_scaleY + m_offsetY;
        */

        CurveSegment midSegment;
        midSegment.offsetX = -(b / m);
        midSegment.offsetY = 0.0f;
        midSegment.scaleX = 1.0f;
        midSegment.scaleY = 1.0f;
        midSegment.lnA = g * logf(m);
        midSegment.B = g;

        m_segments[1] = midSegment;

        toeM = EvalDerivativeLinearGamma(m, b, g, params.x0);
        shoulderM = EvalDerivativeLinearGamma(m, b, g, params.x1);

        // apply gamma to endpoints
        params.y0 = max(1e-5f, powf(params.y0, params.gamma));
        params.y1 = max(1e-5f, powf(params.y1, params.gamma));

        params.overshootY = powf(1.0f + params.overshootY, params.gamma) - 1.0f;
    }

    x0 = params.x0;
    x1 = params.x1;
    y0 = params.y0;
    y1 = params.y1;

    // toe section
    {
        CurveSegment toeSegment;
        toeSegment.offsetX = 0;
        toeSegment.offsetY = 0.0f;
        toeSegment.scaleX = 1.0f;
        toeSegment.scaleY = 1.0f;

        SolveAB(toeSegment.lnA, toeSegment.B, params.x0, params.y0, toeM);
        m_segments[0] = toeSegment;
    }

    // shoulder section
    {
        // use the simple version that is usually too flat
        CurveSegment shoulderSegment;

        float x0 = (1.0f + params.overshootX) - params.x1;
        float y0 = (1.0f + params.overshootY) - params.y1;

        float lnA = 0.0f;
        float B = 0.0f;
        SolveAB(lnA, B, x0, y0, shoulderM);

        shoulderSegment.offsetX = (1.0f + params.overshootX);
        shoulderSegment.offsetY = (1.0f + params.overshootY);

        shoulderSegment.scaleX = -1.0f;
        shoulderSegment.scaleY = -1.0f;
        shoulderSegment.lnA = lnA;
        shoulderSegment.B = B;

        m_segments[2] = shoulderSegment;
    }

    // Normalize so that we hit 1.0 at our white point. We wouldn't have do this if we
    // skipped the overshoot part.
    {
        // evaluate shoulder at the end of the curve
        float scale = m_segments[2].eval(1.0f);
        float invScale = 1.0f / scale;

        m_segments[0].offsetY *= invScale;
        m_segments[0].scaleY *= invScale;

        m_segments[1].offsetY *= invScale;
        m_segments[1].scaleY *= invScale;

        m_segments[2].offsetY *= invScale;
        m_segments[2].scaleY *= invScale;
    }
}

void FilmicMapping::getDirectParams(DirectParams& dstParams, const UserParams& srcParams) {
    dstParams = DirectParams();

    float toeStrength = srcParams.toeStrength;
    float toeLength = srcParams.toeLength;
    float shoulderStrength = srcParams.shoulderStrength;
    float shoulderLength = srcParams.shoulderLength;

    float shoulderAngle = srcParams.shoulderAngle;
    float gamma = srcParams.gamma;

    // This is not actually the display gamma. It's just a UI space to avoid having to
    // enter small numbers for the input.
    float perceptualGamma = 2.2f;

    // constraints
    {
        toeLength = powf(clamp(toeLength, 0.f, 1.f), perceptualGamma);
        toeStrength = clamp(toeStrength, 0.f, 1.f);
        shoulderAngle = clamp(shoulderAngle, 0.f, 1.f);
        shoulderLength = max(1e-5f, clamp(shoulderLength, 0.f, 1.f));

        shoulderStrength = max(0.0f, shoulderStrength);
    }

    // apply base params
    {
        // toe goes from 0 to 0.5
        float x0 = toeLength * .5f;
        float y0 = (1.0f - toeStrength) * x0; // lerp from 0 to x0

        float remainingY = 1.0f - y0;

        float initialW = x0 + remainingY;

        float y1_offset = (1.0f - shoulderLength) * remainingY;
        float x1 = x0 + y1_offset;
        float y1 = y0 + y1_offset;

        // filmic shoulder strength is in F stops
        float extraW = exp2f(shoulderStrength) - 1.0f;

        float W = initialW + extraW;

        dstParams.x0 = x0;
        dstParams.y0 = y0;
        dstParams.x1 = x1;
        dstParams.y1 = y1;
        dstParams.W = W;

        // bake the linear to gamma space conversion
        dstParams.gamma = gamma;
    }

    dstParams.overshootX = (dstParams.W * 2.0f) * shoulderAngle * shoulderStrength;
    dstParams.overshootY = 0.5f * shoulderAngle * shoulderStrength;
}


NAMESPACE_SPH_END
