#include "gui/Utils.h"
#include "common/Assert.h"
#include "io/FileSystem.h"
#include "thread/CheckFunction.h"
#include <iomanip>
#include <wx/dcmemory.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>

NAMESPACE_SPH_BEGIN

static String getDesc(ArrayView<const FileFormat> formats, const bool doAll) {
    String desc;
    bool isFirst = true;
    if (doAll && formats.size() > 1) {
        desc += L"All supported formats|";
        for (const FileFormat& format : formats) {
            desc += L"*." + format.extension + L";";
        }
        isFirst = false;
    }
    for (const FileFormat& format : formats) {
        if (!isFirst) {
            desc += "|";
        }
        isFirst = false;
        desc += format.description + L" (*." + format.extension + L")|*." + format.extension;
    }
    return desc;
}

static Optional<std::pair<Path, int>> doFileDialog(const String& title,
    const String& fileMask,
    String& defaultDir,
    const int flags) {
    wxFileDialog dialog(nullptr, title.toUnicode(), "", defaultDir.toUnicode(), fileMask.toUnicode(), flags);
    if (dialog.ShowModal() == wxID_CANCEL) {
        return NOTHING;
    }
    Path path(dialog.GetPath().wc_str());
    defaultDir = path.parentPath().string();
    return std::make_pair(path, !fileMask.empty() ? dialog.GetFilterIndex() : -1);
}

Optional<Path> doOpenFileDialog(const String& title, Array<FileFormat>&& formats) {
    static String defaultDir = "";
    Optional<std::pair<Path, int>> pathAndIndex =
        doFileDialog(title, getDesc(formats, true), defaultDir, wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (pathAndIndex) {
        return pathAndIndex->first;
    } else {
        return NOTHING;
    }
}

Optional<Path> doSaveFileDialog(const String& title, Array<FileFormat>&& formats) {
    static String defaultDir = "";
    Optional<std::pair<Path, int>> pathAndIndex =
        doFileDialog(title, getDesc(formats, false), defaultDir, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (pathAndIndex) {
        const int index = pathAndIndex->second;
        if (index >= 0) {
            const String& ext = formats[index].extension;
            pathAndIndex->first.replaceExtension(ext);
        }
        return pathAndIndex->first;
    } else {
        return NOTHING;
    }
}

int messageBox(const String& message, const String& title, const int buttons) {
    return wxMessageBox(message.toUnicode(), title.toUnicode(), buttons);
}

static Size getSubscriptSize(const String& text) {
    Size size = 0;
    if (!text.empty() && text[0] == L'-') {
        size++;
    }
    for (; size < text.size(); ++size) {
        const wchar_t c = text[size];
        if ((c < L'0' || c > L'9') && (c < L'a' || c > L'z') && (c < L'A' || c > L'Z')) {
            return size;
        }
    }
    return text.size();
}

void drawTextWithSubscripts(wxDC& dc, const String& text, const wxPoint point) {
    std::size_t n;
    std::size_t m = 0;
    wxPoint actPoint = point;
    const wxFont font = dc.GetFont();
    const wxFont subcriptFont = font.Smaller();

    StaticArray<wchar_t, 2> specialChars = { L'_', L'^' };
    while ((n = text.findAny(specialChars, m)) != String::npos) {
        const bool isSubscript = text[n] == '_';
        String part = text.substr(m, n - m);
        wxSize extent = dc.GetTextExtent(part.toUnicode());
        // draw part up to subscript using current font
        dc.DrawText(part.toUnicode(), actPoint);

        actPoint.x += extent.x;
        const Size subscriptSize = getSubscriptSize(text.substr(n + 1));
        const String subscript = text.substr(n + 1, subscriptSize);
        dc.SetFont(subcriptFont);
        wxPoint subscriptPoint = isSubscript ? wxPoint(actPoint.x + 2, actPoint.y + extent.y / 3)
                                             : wxPoint(actPoint.x + 2, actPoint.y - extent.y / 4);

        dc.DrawText(subscript.toUnicode(), subscriptPoint);
        actPoint.x = subscriptPoint.x + dc.GetTextExtent(subscript.toUnicode()).x;

        dc.SetFont(font);
        m = n + 1 + subscriptSize; // skip _ and the subscript character
    }
    // draw last part of the text
    dc.DrawText(text.substr(m).toUnicode(), actPoint);
}

String toPrintableString(const Float value, const Size precision, const Float decimalThreshold) {
    const Float absValue = abs(value);
    std::wstringstream ss;
    if (absValue == 0._f || (absValue >= 1._f / decimalThreshold && absValue <= decimalThreshold)) {
        ss << value;
    } else {
        ss << std::setprecision(precision) << std::scientific << value;
    }
    String s = String::fromWstring(ss.str());

    String printable;
    if (value > 0) {
        printable += ' ';
    }
    bool exponent = false;
    for (Size i = 0; i < s.size(); ++i) {
        // replace unary pluses with spaces (to keep alignment of numbers
        if (s[i] == '+') {
            continue;
        }
        // replace 'e' with 'x10^'
        if (s[i] == 'e') {
            exponent = true;
            printable += L"\u00D710^";
            continue;
        }
        // get rid of leading zeros in exponent
        if (exponent) {
            if (s[i] == '-') {
                printable += '-';
                continue;
            }
            if (s[i] == '0') {
                continue;
            }
        }
        printable += s[i];
        exponent = false;
    }
    return printable;
}

static Pixel getOriginOffset(wxDC& dc, Flags<TextAlign> align, const String& text) {
    wxSize extent = dc.GetTextExtent(text.toUnicode());
    if (text.find(L"^") != String::npos) {
        // number with superscript is actually a bit shorter, shrink it
        /// \todo this should be done more correctly
        extent.x -= 6;
    }
    Pixel offset(0, 0);
    if (align.has(TextAlign::LEFT)) {
        offset.x -= extent.x;
    }
    if (align.has(TextAlign::HORIZONTAL_CENTER)) {
        offset.x -= extent.x / 2;
    }
    if (align.has(TextAlign::TOP)) {
        offset.y -= extent.y;
    }
    if (align.has(TextAlign::VERTICAL_CENTER)) {
        offset.y -= extent.y / 2;
    }
    return offset;
}

void printLabels(wxDC& dc, ArrayView<const IRenderOutput::Label> labels) {
    wxFont font = dc.GetFont();
    for (const IRenderOutput::Label& label : labels) {
        dc.SetTextForeground(wxColour(label.color));
        font.SetPointSize(label.fontSize);
        dc.SetFont(font);
        const wxPoint origin(label.position + getOriginOffset(dc, label.align, label.text));
        drawTextWithSubscripts(dc, label.text, origin);
    }
}

TransparencyPattern::TransparencyPattern(const Size side, const Rgba& dark, const Rgba& light) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    stipple.resize(Pixel(2 * side, 2 * side), dark);
    for (Size y = 0; y < side; ++y) {
        for (Size x = 0; x < side; ++x) {
            stipple(x, y) = light;
            stipple(x + side, y + side) = light;
        }
    }
}

void TransparencyPattern::draw(wxDC& dc, const wxRect& rect) const {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    wxBrush brush = *wxBLACK_BRUSH;
    wxBitmap wx;
    toWxBitmap(stipple, wx);
    brush.SetStipple(wx);
    dc.SetBrush(brush);
    dc.DrawRectangle(rect);
}

NAMESPACE_SPH_END
