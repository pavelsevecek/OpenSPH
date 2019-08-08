#pragma once

#include "run/VirtualSettings.h"

NAMESPACE_SPH_BEGIN


enum class IntervalBound {
    LOWER,
    UPPER,
};

template <typename TEnum>
class IntervalEntry : public IVirtualEntry {
private:
    Settings<TEnum>& settings;
    TEnum id;
    std::string name;
    IntervalBound bound;


public:
    IntervalEntry(Settings<TEnum>& settings,
        const TEnum id,
        const std::string& name,
        const IntervalBound bound)
        : settings(settings)
        , id(id)
        , name(name)
        , bound(bound) {}

    virtual bool enabled() const override {
        return true;
    }

    virtual void set(const Value& value) override {
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

    virtual std::string getName() const override {
        return name;
    }
};

template <typename TEnum>
INLINE AutoPtr<IVirtualEntry> makeEntry(Settings<TEnum>& settings,
    const TEnum id,
    const std::string& name,
    const IntervalBound bound) {
    return makeAuto<IntervalEntry<TEnum>>(settings, id, name, bound);
}

NAMESPACE_SPH_END
