#pragma once

#include "gui/Controller.h"
#include "io/FileSystem.h"
#include "thread/CheckFunction.h"
#include <map>
#include <wx/dcclient.h>
#include <wx/panel.h>
#include <wx/sizer.h>

NAMESPACE_SPH_BEGIN

std::map<int, Path> getSequenceFiles(const Path& inputPath);

class ITimeLineCallbacks : public Polymorphic {
public:
    virtual void frameChanged(const Path& newFile) const = 0;

    virtual void startSequence(const Path& firstFile) const = 0;

    virtual void stop() const = 0;

    virtual void pause() const = 0;
};


class TimeLinePanel : public wxPanel {
private:
    SharedPtr<ITimeLineCallbacks> callbacks;

    std::map<int, Path> fileMap;
    int currentFrame = 0;
    int mouseFrame = 0;

public:
    TimeLinePanel(wxWindow* parent, const Path& inputFile, SharedPtr<ITimeLineCallbacks> callbacks);

    void update(const Path& inputFile);

    void setFrame(const Size newFrame);

    void setNext();

    void setPrevious();

    void startSequence();

private:
    int positionToFrame(const wxPoint position) const;

    void reload();

    void onPaint(wxPaintEvent& evt);

    void onMouseMotion(wxMouseEvent& evt);

    void onLeftClick(wxMouseEvent& evt);

    void onKeyUp(wxKeyEvent& evt);
};

class TimeLine : public wxPanel {
private:
    TimeLinePanel* timeline;

public:
    TimeLine(wxWindow* parent, const Path& inputFile, SharedPtr<ITimeLineCallbacks> callbacks);

    void update(const Path& inputFile) {
        timeline->update(inputFile);
    }

    void setFrame(const Size newFrame) {
        timeline->setFrame(newFrame);
    }
};

NAMESPACE_SPH_END
