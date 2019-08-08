#pragma once

#include "gui/Utils.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Color.h"
#include "gui/renderers/IRenderer.h"
#include "sph/kernel/Kernel.h"
#include <wx/dc.h>

NAMESPACE_SPH_BEGIN


enum class ColorFlag {
    LINE = 1 << 0,
    FILL = 1 << 1,
    TEXT = 1 << 2,
};

/// \brief Abstraction of a device used for rendering.
///
/// The interface is currently quite minimalistic, can be extended if needed.
class IRenderContext : public Polymorphic {
public:
    /// \brief Returns the size of the canvas associated with the context.
    virtual Pixel size() const = 0;

    /// \brief Selects the color for one or more drawing modes.
    virtual void setColor(const Rgba& color, const Flags<ColorFlag> flags) = 0;

    /// \brief Modifies the thickness of the lines.
    virtual void setThickness(const float thickness) = 0;

    virtual void setFontSize(const int fontSize) = 0;

    /// \brief Fills the whole canvas with given color.
    ///
    /// This erases any previous content.
    virtual void fill(const Rgba& color) = 0;

    /// \brief Draws a line connecting two points.
    virtual void drawLine(const Coords p1, const Coords p2) = 0;

    /// \brief Draws a circle, given its center and a radius.
    virtual void drawCircle(const Coords center, const float radius) = 0;

    /// \brief Draws a triangle given three points.
    virtual void drawTriangle(const Coords p1, const Coords p2, const Coords p3) = 0;

    /// \brief Draws a bitmap, given the position of its left-top corner.
    virtual void drawBitmap(const Coords p, const Bitmap<Rgba>& bitmap) = 0;

    virtual void drawText(const Coords p, const Flags<TextAlign> align, const std::string& s) = 0;

    virtual void drawText(const Coords p, const Flags<TextAlign> align, const std::wstring& s) = 0;
};


class PreviewRenderContext : public IRenderContext {
protected:
    Bitmap<Rgba>& bitmap;

    Array<IRenderOutput::Label> labels;

    struct {
        Rgba line = Rgba::black();
        Rgba fill = Rgba::black();
        Rgba text = Rgba::white();
    } colors;

    float thickness = 1._f;

    int fontSize = 9;

public:
    explicit PreviewRenderContext(Bitmap<Rgba>& bitmap)
        : bitmap(bitmap) {}

    Array<IRenderOutput::Label> getLabels() const {
        return labels.clone();
    }

    virtual Pixel size() const override {
        return bitmap.size();
    }

    virtual void setColor(const Rgba& color, const Flags<ColorFlag> flags) override;

    virtual void setThickness(const float newThickness) override;

    virtual void setFontSize(const int newFontSize) override;

    virtual void fill(const Rgba& color) override;

    virtual void drawLine(const Coords p1, const Coords p2) override;

    virtual void drawCircle(const Coords center, const float radius) override;

    virtual void drawTriangle(const Coords p1, const Coords p2, const Coords p3) override;

    virtual void drawBitmap(const Coords p, const Bitmap<Rgba>& subBitmap) override;

    virtual void drawText(const Coords p, const Flags<TextAlign> align, const std::string& s) override;

    virtual void drawText(const Coords p, const Flags<TextAlign> align, const std::wstring& s) override;

private:
    void drawSafe(const Pixel p, const Rgba c) {
        if (p.x >= 0 && p.y >= 0 && p.x < bitmap.size().x && p.y < bitmap.size().y) {
            bitmap[Pixel(p)] = c.over(bitmap[Pixel(p)]);
        }
    }

    void drawSafe(const Coords p, const Rgba c) {
        this->drawSafe(Pixel(p), c);
    }
};

/// \todo do not derive from PreviewRenderContext, rather some RenderContextBase; AA context is NOT a preview
/// render context!
class AntiAliasedRenderContext : public PreviewRenderContext {
public:
    AntiAliasedRenderContext(Bitmap<Rgba>& bitmap)
        : PreviewRenderContext(bitmap) {}

    virtual void drawCircle(const Coords center, const float radius) override;

protected:
    void drawSafe(const Pixel p, const Rgba c) {
        if (p.x >= 0 && p.y >= 0 && p.x < bitmap.size().x && p.y < bitmap.size().y) {
            bitmap[Pixel(p)] = c.over(bitmap[Pixel(p)]);
        }
    }
};


class SmoothedRenderContext : public AntiAliasedRenderContext {
private:
    LutKernel<2> kernel;

public:
    SmoothedRenderContext(Bitmap<Rgba>& bitmap, const LutKernel<2>& kernel)
        : AntiAliasedRenderContext(bitmap)
        , kernel(kernel) {}

    virtual void drawCircle(const Coords center, const float radius) override;
};


/// \brief Render context drawing directly into wxDC.
///
/// Must be used only in main thread!!
class WxRenderContext : public IRenderContext {
private:
    wxDC& dc;
    wxPen pen;
    wxBrush brush;

public:
    WxRenderContext(wxDC& dc)
        : dc(dc) {
        pen = dc.GetPen();
        brush = dc.GetBrush();
    }

    virtual Pixel size() const override {
        const wxSize s = dc.GetSize();
        return Pixel(s.x, s.y);
    }

    virtual void setColor(const Rgba& color, const Flags<ColorFlag> flags) override {
        if (flags.has(ColorFlag::LINE)) {
            pen.SetColour(wxColour(color));
            dc.SetPen(pen);
        }
        if (flags.has(ColorFlag::FILL)) {
            brush.SetColour(wxColour(color));
            dc.SetBrush(brush);
        }
        if (flags.has(ColorFlag::TEXT)) {
            dc.SetTextForeground(wxColour(color));
        }
    }

    virtual void setThickness(const float UNUSED(newThickness)) override { /// \todo
    }

    virtual void setFontSize(const int newFontSize) override {
        wxFont font = dc.GetFont();
        font.SetPointSize(newFontSize);
        dc.SetFont(font);
    }

    virtual void fill(const Rgba& color) override {
        brush.SetColour(wxColour(color));
        dc.SetBrush(brush);
        dc.DrawRectangle(wxPoint(0, 0), dc.GetSize());
    }

    virtual void drawLine(const Coords p1, const Coords p2) override {
        dc.DrawLine(wxPoint(p1), wxPoint(p2));
    }

    virtual void drawCircle(const Coords center, const float radius) override {
        dc.DrawCircle(wxPoint(center), int(radius));
    }

    virtual void drawTriangle(const Coords, const Coords, const Coords) override {
        NOT_IMPLEMENTED;
    }

    virtual void drawBitmap(const Coords, const Bitmap<Rgba>&) override {
        NOT_IMPLEMENTED;
    }

    virtual void drawText(const Coords p, const Flags<TextAlign> align, const std::string& s) override {
        std::wstring ws(s.begin(), s.end());
        this->drawText(p, align, ws);
    }

    virtual void drawText(const Coords p, const Flags<TextAlign> align, const std::wstring& s) override {
        IRenderOutput::Label label;
        label.text = s;
        label.align = align;
        label.fontSize = dc.GetFont().GetPointSize();
        label.color = Rgba(dc.GetTextForeground());
        label.position = Pixel(p);
        printLabels(dc, Array<IRenderOutput::Label>{ label });
    }
};

NAMESPACE_SPH_END
