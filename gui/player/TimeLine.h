#pragma once

#include "gui/Controller.h"
#include "io/FileSystem.h"
#include "thread/CheckFunction.h"
#include <map>
#include <wx/dcclient.h>
#include <wx/panel.h>
#include <wx/sizer.h>

NAMESPACE_SPH_BEGIN

inline std::map<int, Path> getSequenceFiles(const Path& inputPath) {
    Path fileMask;
    std::map<int, Path> fileMap;
    OutputFile outputFile(inputPath);
    if (outputFile.hasWildcard()) {
        // already a mask
        fileMask = inputPath;
    } else {
        Optional<OutputFile> deducedFile = OutputFile::getMaskFromPath(inputPath);
        if (!deducedFile) {
            // just a single file, not part of a sequence (e.g. frag_final.ssf)
            fileMap.insert(std::make_pair(0, inputPath));
            return fileMap;
        }
        fileMask = deducedFile->getMask();
    }

    const Path dir = fileMask.parentPath();
    Array<Path> files = FileSystem::getFilesInDirectory(dir);

    for (Path& file : files) {
        const Optional<OutputFile> deducedMask = OutputFile::getMaskFromPath(dir / file);
        if (deducedMask && deducedMask->getMask() == fileMask) {
            const Optional<Size> index = OutputFile::getDumpIdx(dir / file);
            ASSERT(index);
            fileMap[index.value()] = dir / file;
        }
    }

    ASSERT(!fileMap.empty());
    return fileMap;
}


class TimeLinePanel : public wxPanel {
private:
    Function<void(Path)> onFrameChanged;

    std::map<int, Path> fileMap;
    int currentFrame = 0;
    int mouseFrame = 0;

public:
    TimeLinePanel(wxWindow* parent, const Path& inputFile, Function<void(Path)> onFrameChanged)
        : wxPanel(parent, wxID_ANY)
        , onFrameChanged(onFrameChanged) {

        this->update(inputFile);

        this->SetMinSize(wxSize(1024, 50));
        this->Connect(wxEVT_PAINT, wxPaintEventHandler(TimeLinePanel::onPaint));
        this->Connect(wxEVT_MOTION, wxMouseEventHandler(TimeLinePanel::onMouseMotion));
        this->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(TimeLinePanel::onLeftClick));
        this->Connect(wxEVT_KEY_UP, wxKeyEventHandler(TimeLinePanel::onKeyUp));
    }

    void setFrame(const int newFrame) {
        currentFrame = newFrame;
        this->Refresh();
    }

    void update(const Path& inputFile) {
        fileMap = getSequenceFiles(inputFile);
        OutputFile of(inputFile);
        if (!of.hasWildcard()) {
            currentFrame = OutputFile::getDumpIdx(inputFile).valueOr(0);
        }
    }

private:
    int positionToFrame(const wxPoint position) const {
        wxSize size = this->GetSize();
        const int firstFrame = fileMap.begin()->first;
        const int lastFrame = fileMap.rbegin()->first;
        const int frame = firstFrame + int(roundf(float(position.x) * (lastFrame - firstFrame) / size.x));
        const auto upperIter = fileMap.upper_bound(frame);
        if (upperIter == fileMap.begin()) {
            return upperIter->first;
        } else {
            auto lowerIter = upperIter;
            --lowerIter;

            if (upperIter == fileMap.end()) {
                return lowerIter->first;
            } else {
                // return the closer frame
                const int lowerDist = frame - lowerIter->first;
                const int upperDist = upperIter->first - frame;
                return (upperDist < lowerDist) ? upperIter->first : lowerIter->first;
            }
        }
    }

    void onPaint(wxPaintEvent& UNUSED(evt)) {
        wxPaintDC dc(this);
        const wxSize size = dc.GetSize();
        /// \todo deduplicate
        Rgba backgroundColor = Rgba(this->GetParent()->GetBackgroundColour());
        wxPen pen = *wxBLACK_PEN;
        pen.SetWidth(2);
        wxBrush brush;
        wxColour fillColor(backgroundColor.darken(0.3f));
        brush.SetColour(fillColor);
        pen.SetColour(fillColor);

        dc.SetBrush(brush);
        dc.SetPen(pen);
        dc.DrawRectangle(wxPoint(0, 0), size);
        dc.SetTextForeground(wxColour(255, 255, 255));
        wxFont font = dc.GetFont();
        font.MakeSmaller();
        dc.SetFont(font);

        const int fileCnt = fileMap.size();
        if (fileCnt == 1) {
            // nothing to draw
            return;
        }

        // ad hoc stepping
        int step = 1;
        if (fileCnt > 60) {
            step = int(fileCnt / 60) * 5;
        } else if (fileCnt > 30) {
            step = 2;
        }

        const int firstFrame = fileMap.begin()->first;
        const int lastFrame = fileMap.rbegin()->first;
        int i = 0;
        for (auto frameAndPath : fileMap) {
            const int frame = frameAndPath.first;
            bool keyframe = (i % step == 0);
            bool doFull = keyframe;
            if (frame == currentFrame) {
                pen.SetColour(wxColour(255, 80, 0));
                doFull = true;
            } else if (frame == mouseFrame) {
                pen.SetColour(wxColour(128, 128, 128));
                doFull = true;
            } else {
                // pen.SetColour(wxColour(20, 20, 20));
                pen.SetColour(wxColour(backgroundColor));
            }
            dc.SetPen(pen);
            const int x = (frame - firstFrame) * size.x / (lastFrame - firstFrame);
            if (doFull) {
                dc.DrawLine(wxPoint(x, 0), wxPoint(x, size.y));
            } else {
                dc.DrawLine(wxPoint(x, 0), wxPoint(x, 5));
                dc.DrawLine(wxPoint(x, size.y - 5), wxPoint(x, size.y));
            }

            if (keyframe) {
                const std::string text = std::to_string(frame);
                const wxSize extent = dc.GetTextExtent(text);
                if (x + extent.x + 3 < size.x) {
                    dc.DrawText(text, wxPoint(x + 3, size.y - 20));
                }
            }
            ++i;
        }
    }

    void onMouseMotion(wxMouseEvent& evt) {
        mouseFrame = positionToFrame(evt.GetPosition());
        this->Refresh();
    }

    void onLeftClick(wxMouseEvent& evt) {
        currentFrame = positionToFrame(evt.GetPosition());
        reload();
    }

    void onKeyUp(wxKeyEvent& evt) {
        switch (evt.GetKeyCode()) {
        case WXK_LEFT: {
            auto iter = fileMap.find(currentFrame);
            ASSERT(iter != fileMap.end());
            if (iter != fileMap.begin()) {
                --iter;
                currentFrame = iter->first;
                reload();
            }
            break;
        }
        case WXK_RIGHT: {
            auto iter = fileMap.find(currentFrame);
            ASSERT(iter != fileMap.end());
            ++iter;
            if (iter != fileMap.end()) {
                currentFrame = iter->first;
                reload();
            }
            break;
        }
        default:
            break;
        }
    }

    void reload() {
        onFrameChanged(fileMap[currentFrame]);
    }
};

class TimeLinePlugin : public IPluginControls {
private:
    Path fileMask;
    Function<void(Path)> onFrameChanged;

    TimeLinePanel* panel;

public:
    TimeLinePlugin(const Path& fileMask, Function<void(Path)> onFrameChanged)
        : fileMask(fileMask)
        , onFrameChanged(onFrameChanged) {}

    virtual void create(wxWindow* parent, wxSizer* sizer) override {
        panel = alignedNew<TimeLinePanel>(parent, fileMask, onFrameChanged);
        sizer->AddSpacer(5);
        sizer->Add(panel);
        sizer->AddSpacer(5);
    }

    virtual void statusChanges(const Path& path, const RunStatus UNUSED(newStatus)) override {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        if (!path.empty() && path != fileMask) {
            fileMask = path;

            // loaded different file
            panel->update(path);
        }
        panel->Refresh();
    }

    void setFrame(const int newFrame) {
        executeOnMainThread([this, newFrame] { panel->setFrame(newFrame); });
    }
};


NAMESPACE_SPH_END
