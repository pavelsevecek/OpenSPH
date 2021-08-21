#pragma once

/// \file SpecialEntries.h
/// \brief Additional bindings to IVirtualSettings
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "math/Curve.h"
#include "run/VirtualSettings.h"

NAMESPACE_SPH_BEGIN

enum class IntervalBound {
    LOWER,
    UPPER,
};

/// \brief Entry connecting to either lower or upper bound of an interval stored in settings.
template <typename TEnum>
class IntervalEntry : public EntryControl {
private:
    Settings<TEnum>& settings;
    TEnum id;
    String name;
    IntervalBound bound;

public:
    IntervalEntry(Settings<TEnum>& settings, const TEnum id, const String& name, const IntervalBound bound)
        : settings(settings)
        , id(id)
        , name(name)
        , bound(bound) {}

    virtual void setImpl(const Value& value) override {
        const Interval i = settings.template get<Interval>(id);
        Float lower = i.lower();
        Float upper = i.upper();
        if (bound == IntervalBound::LOWER) {
            lower = value.get<Float>();
        } else {
            upper = value.get<Float>();
        }
        settings.set(id, Interval(lower, upper));
    }

    virtual Value get() const override {
        const Interval i = settings.template get<Interval>(id);
        if (bound == IntervalBound::LOWER) {
            return i.lower();
        } else {
            return i.upper();
        }
    }

    virtual Type getType() const override {
        return IVirtualEntry::Type::FLOAT;
    }

    virtual String getName() const override {
        return name;
    }
};

template <typename TEnum>
INLINE AutoPtr<IVirtualEntry> makeEntry(Settings<TEnum>& settings,
    const TEnum id,
    const String& name,
    const IntervalBound bound) {
    return makeAuto<IntervalEntry<TEnum>>(settings, id, name, bound);
}

/// \brief Special entry allowing to access and (de)serialize a curve.
class CurveEntry : public IExtraEntry {
private:
    Curve curve;

public:
    CurveEntry() = default;

    CurveEntry(const Curve& curve)
        : curve(curve) {}

    Curve getCurve() const {
        return curve;
    }

    virtual String toString() const override;

    virtual void fromString(const String& s) override;

    virtual AutoPtr<IExtraEntry> clone() const override;
};


NAMESPACE_SPH_END
