#include "gui/objects/Plots.h"
#include "gui/objects/Colorizer.h"
#include "io/Path.h"
#include "post/Point.h"
#include <fstream>

NAMESPACE_SPH_BEGIN

class SelectedParticleIntegral : public IIntegral<Float> {
private:
    SharedPtr<IColorizer> colorizer;
    Size selectedIdx;

public:
    SelectedParticleIntegral(SharedPtr<IColorizer> colorizer, const Size idx)
        : colorizer(colorizer)
        , selectedIdx(idx) {}

    virtual Float evaluate(const Storage& UNUSED(storage)) const override {
        SPH_ASSERT(colorizer->isInitialized(), colorizer->name());
        return colorizer->evalScalar(selectedIdx).valueOr(0.f);
    }

    virtual String getName() const override {
        return colorizer->name() + " " + toString(selectedIdx);
    }
};

SelectedParticlePlot::SelectedParticlePlot(const Float initialPeriod)
    : initialPeriod(initialPeriod) {}

SelectedParticlePlot::~SelectedParticlePlot() = default;

void SelectedParticlePlot::selectParticle(const Optional<Size> idx) {
    if (selectedIdx.valueOr(-1) == idx.valueOr(-1)) {
        // either the same particle or deselecting when nothing was selected; do nothing
        return;
    }
    if (selectedIdx && currentPlot) {
        // save the current plot to cache
        plotCache.insert(selectedIdx.value(), currentPlot);
    }
    selectedIdx = idx;

    if (idx) {
        // try to get the plot from the cache
        Optional<SharedPtr<TemporalPlot>&> cachedPlot = plotCache.tryGet(idx.value());
        if (cachedPlot) {
            // reuse the cached plot
            currentPlot = cachedPlot.value();
            return;
        }
    }
    // either deselecting or no cached plot found; clear the plot
    this->clear();
}

void SelectedParticlePlot::setColorizer(const SharedPtr<IColorizer>& newColorizer) {
    if (colorizer != newColorizer) {
        colorizer = newColorizer;
        this->clear();
        plotCache.clear();
    }
}

String SelectedParticlePlot::getCaption() const {
    if (currentPlot) {
        return currentPlot->getCaption();
    } else {
        return "Selected particle";
    }
}

void SelectedParticlePlot::onTimeStep(const Storage& storage, const Statistics& stats) {
    if (selectedIdx && currentPlot) {
        currentPlot->onTimeStep(storage, stats);
    } else {
        currentPlot.reset();
    }
    // also do onTimeStep for all cached plots
    for (auto& plot : plotCache) {
        plot.value()->onTimeStep(storage, stats);
    }
    this->syncRanges();
}

void SelectedParticlePlot::clear() {
    if (selectedIdx) {
        AutoPtr<SelectedParticleIntegral> integral =
            makeAuto<SelectedParticleIntegral>(colorizer, selectedIdx.value());
        TemporalPlot::Params params;
        params.minRangeY = EPS;
        params.shrinkY = false;
        params.period = initialPeriod;
        params.maxPointCnt = 1000;
        currentPlot = makeAuto<TemporalPlot>(std::move(integral), params);
    } else {
        currentPlot.reset();
    }
    this->syncRanges();
}

void SelectedParticlePlot::plot(IDrawingContext& dc) const {
    if (currentPlot) {
        currentPlot->plot(dc);
    }
}

class RelativeEnergyChange : public IIntegral<Float> {
private:
    TotalEnergy energy;
    mutable Optional<Float> E_0 = NOTHING;

public:
    RelativeEnergyChange() = default;

    virtual Float evaluate(const Storage& storage) const override {
        const Float E = energy.evaluate(storage);
        if (!E_0 || E_0.value() == 0._f) {
            E_0 = E;
        }
        return E / E_0.value() - 1._f;
    }

    virtual String getName() const override {
        return "Relative energy change";
    }
};

Array<PlotData> getPlotList(const GuiSettings& gui) {
    Array<PlotData> list;

    TemporalPlot::Params params;
    params.minRangeY = 1.4_f;
    params.shrinkY = false;
    params.period = gui.get<Float>(GuiSettingsId::PLOT_INITIAL_PERIOD);

    PlotData data;
    IntegralWrapper integral;
    Flags<PlotEnum> flags = gui.getFlags<PlotEnum>(GuiSettingsId::PLOT_INTEGRALS);

    if (flags.has(PlotEnum::TOTAL_ENERGY)) {
        integral = makeAuto<TotalEnergy>();
        data.plot = makeLocking<TemporalPlot>(integral, params);
        data.color = Rgba(wxColour(240, 255, 80));
        list.push(data);
    }

    if (flags.has(PlotEnum::RELATIVE_ENERGY_CHANGE)) {
        TemporalPlot::Params actParams = params;
        actParams.minRangeY = 0.001_f;
        integral = makeAuto<RelativeEnergyChange>();
        data.plot = makeLocking<TemporalPlot>(integral, actParams);
        data.color = Rgba(wxColour(240, 255, 80));
        list.push(data);
    }

    if (flags.has(PlotEnum::KINETIC_ENERGY)) {
        integral = makeAuto<TotalKineticEnergy>();
        data.plot = makeLocking<TemporalPlot>(integral, params);
        data.color = Rgba(wxColour(200, 0, 0));
        list.push(data);
    }

    if (flags.has(PlotEnum::INTERNAL_ENERGY)) {
        integral = makeAuto<TotalInternalEnergy>();
        data.plot = makeLocking<TemporalPlot>(integral, params);
        data.color = Rgba(wxColour(255, 50, 50));
        list.push(data);
    }

    if (flags.has(PlotEnum::TOTAL_MOMENTUM)) {
        integral = makeAuto<TotalMomentum>();
        // params.minRangeY = 1.e6_f;
        data.plot = makeLocking<TemporalPlot>(integral, params);
        data.color = Rgba(wxColour(100, 200, 0));
        list.push(data);
    }

    if (flags.has(PlotEnum::TOTAL_ANGULAR_MOMENTUM)) {
        integral = makeAuto<TotalAngularMomentum>();
        data.plot = makeLocking<TemporalPlot>(integral, params);
        data.color = Rgba(wxColour(130, 80, 255));
        list.push(data);
    }

    String overplotSfd = gui.get<String>(GuiSettingsId::PLOT_OVERPLOT_SFD);

    if (flags.has(PlotEnum::CURRENT_SFD)) {
        Array<AutoPtr<IPlot>> multiplot;
        multiplot.emplaceBack(makeAuto<SfdPlot>(Post::ComponentFlag::OVERLAP, params.period));
        if (!overplotSfd.empty()) {
            multiplot.emplaceBack(getDataPlot(Path(overplotSfd), "Overplot"));
        }
        data.plot = makeLocking<MultiPlot>(std::move(multiplot));
        data.color = Rgba(wxColour(255, 40, 255));
        list.push(data);
    }

    if (flags.has(PlotEnum::PREDICTED_SFD)) {
        /// \todo could be deduplicated a bit
        Array<AutoPtr<IPlot>> multiplot;
        multiplot.emplaceBack(makeAuto<SfdPlot>(Post::ComponentFlag::ESCAPE_VELOCITY, params.period));
        if (!overplotSfd.empty()) {
            multiplot.emplaceBack(getDataPlot(Path(overplotSfd), "overplot"));
        }
        data.plot = makeLocking<MultiPlot>(std::move(multiplot));
        data.color = Rgba(wxColour(80, 150, 255));
        list.push(data);
    }

    if (flags.has(PlotEnum::SPEED_HISTOGRAM)) {
        data.plot = makeLocking<HistogramPlot>(
            Post::HistogramId::VELOCITIES, NOTHING, params.period, "Speed histogram");
        data.color = Rgba(wxColour(40, 100, 150));
        list.push(data);
    }

    if (flags.has(PlotEnum::ANGULAR_HISTOGRAM_OF_VELOCITIES)) {
        data.plot = makeLocking<AngularHistogramPlot>(params.period);
        data.color = Rgba(wxColour(250, 100, 50));
        list.push(data);
    }


    /*if (flags.has(PlotEnum::PERIOD_HISTOGRAM)) {
        data.plot = makeLocking<HistogramPlot>(
            Post::HistogramId::ROTATIONAL_PERIOD, Interval(0._f, 10._f), "Rotational periods");
        plots.push(data.plot);
        data.color = Rgba(wxColour(255, 255, 0));
        list->push(data);
    }

    if (flags.has(PlotEnum::LARGEST_REMNANT_ROTATION)) {
        class LargestRemnantRotation : public IIntegral<Float> {
        public:
            virtual Float evaluate(const Storage& storage) const override {
                ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
                if (!storage.has(QuantityId::ANGULAR_FREQUENCY)) {
                    return 0._f;
                }
                ArrayView<const Vector> omega = storage.getValue<Vector>(QuantityId::ANGULAR_FREQUENCY);

                const Size largestRemnantIdx = std::distance(m.begin(), std::max_element(m.begin(),
    m.end())); const Float w = getLength(omega[largestRemnantIdx]); if (w == 0._f) { return 0._f; } else {
                    return 2._f * PI / (3600._f * w);
                }
            }

            virtual String getName() const override {
                return "Largest remnant period";
            }
        };
        integral = makeAuto<LargestRemnantRotation>();
        auto clonedParams = params;
        clonedParams.minRangeY = 1.e-2_f;
        data.plot = makeLocking<TemporalPlot>(integral, clonedParams);
        plots.push(data.plot);
        data.color = Rgba(wxColour(255, 0, 255));
        list->push(data);
    }*/

    /*    if (flags.has(PlotEnum::SELECTED_PARTICLE)) {
            selectedParticlePlot = makeLocking<SelectedParticlePlot>(params.period);
            data.plot = selectedParticlePlot;
            //plots.push(data.plot);
            data.color = Rgba(wxColour(255, 255, 255));
            list.push(data);
        } else {
            selectedParticlePlot.reset();
        }*/
    return list;
}

AutoPtr<IPlot> getDataPlot(const Path& path, const String& name) {
    std::ifstream is(path.native());
    Array<Post::HistPoint> points;
    while (true) {
        float x, y;
        is >> x >> y;
        if (!is) {
            break;
        }
        points.emplaceBack(Post::HistPoint{ x, Size(y) });
    };
    return makeAuto<DataPlot>(points, AxisScaleEnum::LOG_X | AxisScaleEnum::LOG_Y, name);
}


NAMESPACE_SPH_END
