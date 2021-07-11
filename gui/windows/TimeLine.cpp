#include "windows/TimeLine.h"
#include "windows/Icons.data.h"
#include <wx/bmpbuttn.h>

NAMESPACE_SPH_BEGIN

std::map<int, Path> getSequenceFiles(const Path& inputPath) {
    if (inputPath.empty()) {
        throw Exception("sequence for empty path");
    }

    const Path absolutePath = FileSystem::getAbsolutePath(inputPath);
    Optional<OutputFile> deducedFile = OutputFile::getMaskFromPath(absolutePath);
    if (!deducedFile) {
        // just a single file, not part of a sequence (e.g. frag_final.ssf)
        return { std::make_pair(0, absolutePath) };
    }

    Path fileMask = deducedFile->getMask();
    std::map<int, Path> fileMap;

    const Path dir = absolutePath.parentPath();
    Array<Path> files = FileSystem::getFilesInDirectory(dir);

    for (Path& file : files) {
        const Optional<OutputFile> deducedMask = OutputFile::getMaskFromPath(dir / file);
        // check if part of the same sequence
        if (deducedMask && deducedMask->getMask() == fileMask) {
            const Optional<Size> index = OutputFile::getDumpIdx(dir / file);
            SPH_ASSERT(index);
            fileMap[index.value()] = dir / file;
        }
    }

    if (fileMap.empty()) {
        throw Exception("Cannot open file " + inputPath.native());
    }

    return fileMap;
}

TimeLinePanel::TimeLinePanel(wxWindow* parent, const Path& inputFile, SharedPtr<ITimeLineCallbacks> callbacks)
    : wxPanel(parent, wxID_ANY)
    , callbacks(callbacks) {

    this->update(inputFile);

    this->SetMinSize(wxSize(300, 30));
    this->Connect(wxEVT_PAINT, wxPaintEventHandler(TimeLinePanel::onPaint));
    this->Connect(wxEVT_MOTION, wxMouseEventHandler(TimeLinePanel::onMouseMotion));
    this->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(TimeLinePanel::onLeftClick));
    this->Connect(wxEVT_KEY_UP, wxKeyEventHandler(TimeLinePanel::onKeyUp));
}

void TimeLinePanel::update(const Path& inputFile) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);
    try {
        fileMap = getSequenceFiles(inputFile);
    } catch (const Exception& UNUSED(e)) {
        // we already show one message box in Run, silence the error here
        fileMap.clear();
        currentFrame = 0;
        return;
    }

    OutputFile of(inputFile);
    if (!of.hasWildcard()) {
        currentFrame = OutputFile::getDumpIdx(inputFile).valueOr(0);
    }

    this->Refresh();
}

void TimeLinePanel::setFrame(const Size newFrame) {
    currentFrame = newFrame;
    this->Refresh();
}

void TimeLinePanel::setPrevious() {
    auto iter = fileMap.find(currentFrame);
    SPH_ASSERT(iter != fileMap.end());
    if (iter != fileMap.begin()) {
        --iter;
        currentFrame = iter->first;
        this->reload();
    }
}

void TimeLinePanel::startSequence() {
    callbacks->startSequence(fileMap[currentFrame]);
}

void TimeLinePanel::setNext() {
    auto iter = fileMap.find(currentFrame);
    SPH_ASSERT(iter != fileMap.end());
    ++iter;
    if (iter != fileMap.end()) {
        currentFrame = iter->first;
        this->reload();
    }
}

int TimeLinePanel::positionToFrame(const wxPoint position) const {
    if (fileMap.empty()) {
        return 0;
    }
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

void TimeLinePanel::reload() {
    callbacks->frameChanged(fileMap[currentFrame]);
    this->Refresh();
}

void TimeLinePanel::onPaint(wxPaintEvent& UNUSED(evt)) {
    if (fileMap.empty()) {
        // nothing to do
        return;
    }

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

    const bool isLightTheme = backgroundColor.intensity() > 0.5f;
    if (isLightTheme) {
        dc.SetTextForeground(wxColour(30, 30, 30));
    }

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
            if (isLightTheme) {
                pen.SetColour(wxColour(30, 30, 30));
            } else {
                pen.SetColour(wxColour(backgroundColor));
            }
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

void TimeLinePanel::onMouseMotion(wxMouseEvent& evt) {
    mouseFrame = positionToFrame(evt.GetPosition());
    this->Refresh();
}

void TimeLinePanel::onLeftClick(wxMouseEvent& evt) {
    currentFrame = positionToFrame(evt.GetPosition());
    this->reload();
}

void TimeLinePanel::onKeyUp(wxKeyEvent& evt) {
    switch (evt.GetKeyCode()) {
    case WXK_LEFT:
        this->setPrevious();
        break;
    case WXK_RIGHT:
        this->setNext();
        break;

    default:
        break;
    }
}

static wxBitmapButton* createButton(wxWindow* parent, const wxBitmap& bitmap) {
    const wxSize buttonSize(60, 40);
    wxBitmapButton* button = new wxBitmapButton(parent, wxID_ANY, bitmap);
    button->SetMinSize(buttonSize);
    return button;
}

static wxBitmapButton* createButton(wxWindow* parent, char** data) {
    wxBitmap bitmap(data);
    return createButton(parent, bitmap);
}

TimeLine::TimeLine(wxWindow* parent, const Path& inputFile, SharedPtr<ITimeLineCallbacks> callbacks)
    : wxPanel(parent, wxID_ANY) {
    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

    wxImage nextImage = wxBitmap(nextData).ConvertToImage();
    nextImage = nextImage.Mirror();
    wxBitmapButton* prevButton = createButton(this, wxBitmap(nextImage, wxBITMAP_SCREEN_DEPTH));
    sizer->Add(prevButton, 1, wxALL);

    wxBitmapButton* pauseButton = createButton(this, pauseData);
    sizer->Add(pauseButton, 1, wxALL);

    wxBitmapButton* stopButton = createButton(this, stopData);
    sizer->Add(stopButton, 1, wxALL);

    wxBitmapButton* playButton = createButton(this, playData);
    sizer->Add(playButton, 1, wxALL);

    wxBitmapButton* nextButton = createButton(this, nextData);
    sizer->Add(nextButton, 1, wxALL);

    sizer->AddSpacer(8);
    timeline = new TimeLinePanel(this, inputFile, callbacks);
    sizer->Add(timeline, 40, wxALL | wxEXPAND);

    this->SetSizer(sizer);
    this->Layout();

    prevButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { timeline->setPrevious(); });
    nextButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { timeline->setNext(); });
    playButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { timeline->startSequence(); });
    stopButton->Bind(wxEVT_BUTTON, [callbacks](wxCommandEvent& UNUSED(evt)) { callbacks->stop(); });
    pauseButton->Bind(wxEVT_BUTTON, [callbacks](wxCommandEvent& UNUSED(evt)) { callbacks->pause(); });
}

NAMESPACE_SPH_END
