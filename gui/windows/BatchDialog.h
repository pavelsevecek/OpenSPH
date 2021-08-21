#pragma once

#include "gui/objects/Bitmap.h"
#include "run/Node.h"
#include <wx/dialog.h>
#include <wx/grid.h>

NAMESPACE_SPH_BEGIN

class Config;

class BatchManager {
private:
    struct Col {
        SharedPtr<JobNode> node;
        String key;
    };

    Array<Col> cols;
    Array<String> rows;

    Bitmap<String> cells;

public:
    BatchManager() {
        this->resize(4, 3);
    }

    BatchManager clone() const {
        BatchManager copy;
        copy.cols = cols.clone();
        copy.rows = rows.clone();
        copy.cells = cells.clone();
        return copy;
    }

    Size getRunCount() const {
        return rows.size();
    }

    String getRunName(const Size rowIdx) const {
        if (rows[rowIdx].empty()) {
            return "Run " + toString(rowIdx + 1);
        } else {
            return rows[rowIdx];
        }
    }

    Size getParamCount() const {
        return cols.size();
    }

    String getParamKey(const Size colIdx) const {
        return cols[colIdx].key;
    }

    SharedPtr<JobNode> getParamNode(const Size colIdx) const {
        return cols[colIdx].node;
    }

    String getCell(const Size colIdx, const Size rowIdx) const {
        return cells[Pixel(colIdx, rowIdx)];
    }

    void setRunName(const Size rowIdx, const String& name) {
        rows[rowIdx] = name;
    }

    void setParam(const Size colIdx, const SharedPtr<JobNode>& node, const String& key) {
        cols[colIdx].key = key;
        cols[colIdx].node = node;
    }

    void setCell(const Size colIdx, const Size rowIdx, const String& value) {
        cells[Pixel(colIdx, rowIdx)] = value;
    }

    void resize(const Size rowCnt, const Size colCnt) {
        cols.resize(colCnt);
        rows.resize(rowCnt);
        Bitmap<String> oldCells = cells.clone();
        cells.resize(Pixel(colCnt, rowCnt), "");

        const Size minRowCnt = min(cells.size().y, oldCells.size().y);
        const Size minColCnt = min(cells.size().x, oldCells.size().x);
        for (Size j = 0; j < minRowCnt; ++j) {
            for (Size i = 0; i < minColCnt; ++i) {
                cells[Pixel(i, j)] = oldCells[Pixel(i, j)];
            }
        }
    }

    void duplicateRun(const Size rowIdx) {
        const String runName = rows[rowIdx];
        rows.insert(rowIdx, runName);

        Bitmap<String> newCells(Pixel(cols.size(), rows.size()));
        for (Size j = 0; j < rows.size(); ++j) {
            for (Size i = 0; i < cols.size(); ++i) {
                const Size j0 = j <= rowIdx ? j : j - 1;
                newCells[Pixel(i, j)] = cells[Pixel(i, j0)];
            }
        }
        cells = std::move(newCells);
    }

    void deleteRun(const Size rowIdx) {
        rows.remove(rowIdx);
        Bitmap<String> newCells(Pixel(cols.size(), rows.size()));
        for (Size j = 0; j < rows.size(); ++j) {
            for (Size i = 0; i < cols.size(); ++i) {
                const Size j0 = j <= rowIdx ? j : j + 1;
                newCells[Pixel(i, j)] = cells[Pixel(i, j0)];
            }
        }
        cells = std::move(newCells);
    }

    /// \brief Modifies the settings of the given node hierarchy.
    ///
    /// Nodes are modified according to parameters of given run. Other parameters or nodes not specified in
    /// the manager are unchanged.
    void modifyHierarchy(const Size runIdx, JobNode& node);

    void load(Config& config, ArrayView<const SharedPtr<JobNode>> nodes);
    void save(Config& config);

private:
    void modifyNode(JobNode& node, const Size runIdx, const Size paramIdx);
};

class BatchDialog : public wxDialog {
private:
    BatchManager manager;
    Array<SharedPtr<JobNode>> nodes;

    wxGrid* grid;

public:
    BatchDialog(wxWindow* parent, const BatchManager& manager, Array<SharedPtr<JobNode>>&& nodes);

    ~BatchDialog();

    BatchManager& getBatch() {
        return manager;
    }

private:
    /// Loads grid data from the manager.
    void update();
};

NAMESPACE_SPH_END
