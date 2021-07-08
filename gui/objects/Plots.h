#pragma once

#include "gui/Settings.h"
#include "gui/objects/Color.h"
#include "objects/wrappers/LockingPtr.h"
#include "post/Plot.h"

NAMESPACE_SPH_BEGIN

class IColorizer;

struct PlotData {
    /// Plot to be drawn with associated mutex
    LockingPtr<IPlot> plot;

    /// Color of the plot
    Rgba color;
};


/// \brief Temporal plot of currently selected particle.
///
/// Uses current colorizer as a source quantity. If either colorizer or selected particle changes, plot is
/// cleared. It also holds a cache of previously selected particles, so that if a previously plotted particle
/// is re-selected, the cached data are redrawn.
class SelectedParticlePlot : public IPlot {
private:
    Float initialPeriod;

    // Currently used plot (actual implementation); may be nullptr
    SharedPtr<TemporalPlot> currentPlot;

    // Selected particle; if NOTHING, no plot is drawn
    Optional<Size> selectedIdx;

    // Colorizer used to get the scalar value of the selected particle
    SharedPtr<IColorizer> colorizer;

    // Cache of previously selected particles. Cleared every time a new colorizer is selected.
    FlatMap<Size, SharedPtr<TemporalPlot>> plotCache;

public:
    explicit SelectedParticlePlot(const Float initialPeriod);

    ~SelectedParticlePlot();

    void selectParticle(const Optional<Size> idx);

    void setColorizer(const SharedPtr<IColorizer>& newColorizer);

    virtual std::string getCaption() const override;

    virtual void onTimeStep(const Storage& storage, const Statistics& stats) override;

    virtual void clear() override;

    virtual void plot(IDrawingContext& dc) const override;

private:
    /// Since range getters are not virtual, we have to update ranges manually.
    void syncRanges() {
        if (currentPlot) {
            ranges.x = currentPlot->rangeX();
            ranges.y = currentPlot->rangeY();
        }
    }
};

Array<PlotData> getPlotList(const GuiSettings& gui);

AutoPtr<IPlot> getDataPlot(const Path& path, const std::string& name = "reference");

NAMESPACE_SPH_END
