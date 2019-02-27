#include "objects/RenderContext.h"

NAMESPACE_SPH_BEGIN

void PreviewRenderContext::setColor(const Rgba& color, const Flags<ColorFlag> flags) {
    if (flags.has(ColorFlag::LINE)) {
        colors.line = color;
    }
    if (flags.has(ColorFlag::FILL)) {
        colors.fill = color;
    }
    if (flags.has(ColorFlag::TEXT)) {
        colors.text = color;
    }
    // ASSERT(colors.line.alpha() == 1.f);
}

void PreviewRenderContext::setThickness(const float newThickness) {
    thickness = newThickness;
}

void PreviewRenderContext::setFontSize(const int newFontSize) {
    fontSize = newFontSize;
}

void PreviewRenderContext::fill(const Rgba& color) {
    bitmap.fill(color);
}

void PreviewRenderContext::drawLine(Coords p1, Coords p2) {
    if (abs(p2.x - p1.x) > abs(p2.y - p1.y)) {
        if (p1.x > p2.x) {
            std::swap(p1, p2);
        }
        const float x1 = floor(p1.x);
        const float x2 = ceil(p2.x);
        const float y1 = p1.y;
        const float y2 = p2.y;
        for (int x = x1; x <= x2; ++x) {
            int y = y1 + (x - x1) * (y2 - y1) / (x2 - x1);
            drawSafe(Pixel(x, y), colors.line);
        }
    } else {
        if (p1.y > p2.y) {
            std::swap(p1, p2);
        }
        const float y1 = floor(p1.y);
        const float y2 = ceil(p2.y);
        const float x1 = p1.x;
        const float x2 = p2.x;
        for (int y = y1; y <= y2; ++y) {
            int x = x1 + (y - y1) * (x2 - x1) / (y2 - y1);
            drawSafe(Pixel(x, y), colors.line);
        }
    }
}

void PreviewRenderContext::drawCircle(const Coords center, const float radius) {
    if (center.x < -radius || center.x > bitmap.size().x + radius || center.y < -radius ||
        center.y > bitmap.size().y + radius) {
        return;
    }
    const Pixel p(center);
    const int intRadius(radius);
    for (int y = -intRadius; y <= intRadius; ++y) {
        for (int x = -intRadius; x <= +intRadius; ++x) {
            const int rSqr = sqr(x) + sqr(y);
            if (rSqr <= sqr(radius - 1)) {
                drawSafe(p + Pixel(x, y), colors.fill);
            } else if (rSqr <= sqr(radius)) {
                drawSafe(p + Pixel(x, y), colors.line);
            }
        }
    }
    /*if (size < 0.5_f) {
        // just a single pixel
        dc.DrawPoint(p->point);
    } else if (size < 1._f) {
        // draw a 2x2 square - the circle would come out as 3x3 square
        dc.DrawRectangle(p->point, wxSize(2, 2));
    } else {
        dc.DrawCircle(p->point, round(size));
    }*/
}

void PreviewRenderContext::drawTriangle(const Coords p1, const Coords p2, const Coords p3) {
    // http://www-users.mat.uni.torun.pl/~wrona/3d_tutor/tri_fillers.html

    StaticArray<Coords, 3> p{ p1, p2, p3 };
    std::sort(p.begin(), p.end(), [](Coords p1, Coords p2) { return p1.y < p2.y; });
    ASSERT(p[0].y <= p[1].y && p[1].y <= p[2].y); // sanity check
    Coords a = p[0];
    Coords b = p[1];
    Coords c = p[2];
    a.y--;
    c.y++;

    auto getDx = [](const Coords p1, const Coords p2) {
        if (p2.y - p1.y > 0) {
            return float(p2.x - p1.x) / (p2.y - p1.y);
        } else {
            return 0.f;
        }
    };
    const float dx1 = getDx(a, b);
    const float dx2 = getDx(a, c);
    const float dx3 = getDx(b, c);

    auto doLine = [this](float x1, float x2, float y) {
        if (x1 > x2) {
            std::swap(x1, x2);
        }
        for (int x = floor(x1); x <= ceil(x2); ++x) {
            drawSafe(Pixel(x, int(y)), colors.fill);
        }
    };

    Coords s = a, e = a;
    for (; s.y <= b.y; s.y++, e.y++, s.x += dx2, e.x += dx1) {
        doLine(s.x, e.x, s.y);
    }
    e = b;
    for (; s.y <= c.y; s.y++, e.y++, s.x += dx2, e.x += dx3) {
        doLine(s.x, e.x, s.y);
    }
}

void PreviewRenderContext::drawBitmap(const Coords p, const Bitmap<Rgba>& subBitmap) {
    for (int y = 0; y < subBitmap.size().y; ++y) {
        for (int x = 0; x < subBitmap.size().x; ++x) {
            drawSafe(Pixel(x, y) + Pixel(p), subBitmap[Pixel(x, y)]);
        }
    }
}

void PreviewRenderContext::drawText(const Coords p, const Flags<TextAlign> align, const std::string& s) {
    std::wstring ws(s.begin(), s.end());
    this->drawText(p, align, ws);
}

void PreviewRenderContext::drawText(const Coords p, const Flags<TextAlign> align, const std::wstring& s) {
    labels.push(IRenderOutput::Label{ s, colors.text, fontSize, align, Pixel(p) });
}

void AntiAliasedRenderContext::drawCircle(const Coords center, const float radius) {
    if (center.x < -radius || center.x > bitmap.size().x + radius || center.y < -radius ||
        center.y > bitmap.size().y + radius) {
        return;
    }
    const Pixel p(center);
    for (int y = p.y - radius - 1; y <= p.y + radius + 1; ++y) {
        for (int x = p.x - radius - 1; x <= p.x + radius + 1; ++x) {
            const float distSqr = sqr(x - p.x) + sqr(y - p.y);
            if (distSqr <= sqr(radius + 1)) {
                Rgba color = colors.fill;
                /// \todo  rework -- this somehow needs to take into account the ACTUAL position, not p
                color.a() = clamp(sqr(radius) - distSqr, 0.f, 1.f);
                drawSafe(Pixel(x, y), color);
            }
        }
    }
}

void SmoothedRenderContext::drawCircle(const Coords center, const float radius) {
    if (center.x < -radius || center.x > bitmap.size().x + radius || center.y < -radius ||
        center.y > bitmap.size().y + radius) {
        return;
    }
    const Pixel p(center);
    const float maxRadius = radius * kernel.radius();
    const float normalization = 1.f / kernel.valueImpl(0); // sqr(25.f / particleScale);
    for (int y = p.y - maxRadius - 1; y <= p.y + maxRadius + 1; ++y) {
        for (int x = p.x - maxRadius - 1; x <= p.x + maxRadius + 1; ++x) {
            const float distSqr = sqr(x - center.x) + sqr(y - center.y);
            if (distSqr <= sqr(maxRadius + 1)) {
                Rgba color = colors.fill;

                const float alpha = kernel.valueImpl(distSqr / sqr(radius)) * normalization;
                color.a() = clamp(alpha, 0.f, 1.f);
                drawSafe(Pixel(x, y), color);
            }
        }
    }
}

NAMESPACE_SPH_END
