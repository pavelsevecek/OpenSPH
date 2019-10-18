#pragma once

#include "post/Analysis.h"
#include "quantities/Storage.h"
#include <thread>
#include <wx/grid.h>
#include <wx/panel.h>

NAMESPACE_SPH_BEGIN

class GridPage : public wxPanel {
private:
    wxGrid* grid;
    std::thread thread;

public:
    GridPage(wxWindow* parent, const wxSize size, const wxSize padding);

    ~GridPage();

    void update(const Storage& storage);

private:
    void updateAsync(const Storage& storage);

    template <typename T>
    void updateCell(const Size rowIdx, const Size colIdx, const T& value);
};

NAMESPACE_SPH_END
