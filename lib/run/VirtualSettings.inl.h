#include "run/VirtualSettings.h"

NAMESPACE_SPH_BEGIN

namespace Detail {

template <typename T, typename = void>
class UnitAdapter {
public:
    explicit UnitAdapter(const Float UNUSED(mult)) {}

    INLINE T get(const T input) const {
        return input;
    }

    INLINE T set(const T input) const {
        return input;
    }
};

template <typename T>
class UnitAdapter<T, std::enable_if_t<std::is_same<T, Float>::value || std::is_same<T, Vector>::value>> {
private:
    Float mult;

public:
    explicit UnitAdapter(const Float mult)
        : mult(mult) {}

    INLINE T get(const T input) const {
        return input / mult;
    }

    INLINE T set(const T input) const {
        return input * mult;
    }
};

} // namespace Detail

inline void VirtualSettings::Category::addEntry(const std::string& key, AutoPtr<IVirtualEntry>&& entry) {
    entries.insert(key, std::move(entry));
}

namespace Detail {

template <typename TValue, typename = void>
class ValueEntry : public IVirtualEntry {
private:
    TValue& ref;
    std::string name;
    std::string tooltip;
    Detail::UnitAdapter<TValue> units;

    Function<bool()> enabler = nullptr;

public:
    ValueEntry(TValue& ref,
        const std::string& name,
        const Float mult,
        Function<bool()> enabler,
        const std::string& tooltip)
        : ref(ref)
        , name(name)
        , tooltip(tooltip)
        , units(mult)
        , enabler(enabler) {}

    virtual bool enabled() const override {
        return enabler ? enabler() : true;
    }

    virtual void set(const Value& value) override {
        ref = units.set(value.get<TValue>());
    }

    virtual Value get() const override {
        return units.get(ref);
    }

    virtual Type getType() const override {
        // path has to be included to get correct index of EnumWrapper
        return Type(getTypeIndex<TValue, bool, int, Float, Vector, std::string, Path, EnumWrapper>);
    }

    virtual std::string getName() const override {
        return name;
    }

    virtual std::string getTooltip() const override {
        return tooltip;
    }
};

template <typename TValue>
class ValueEntry<TValue, std::enable_if_t<FlagsTraits<TValue>::isFlags>> : public IVirtualEntry {
private:
    TValue& ref;
    std::string name;
    std::string tooltip;
    Function<bool()> enabler;

    using TEnum = typename FlagsTraits<TValue>::Type;

public:
    ValueEntry(TValue& ref,
        const std::string& name,
        const Float UNUSED(mult),
        Function<bool()> enabler,
        const std::string& tooltip)
        : ref(ref)
        , name(name)
        , tooltip(tooltip)
        , enabler(enabler) {}

    virtual bool enabled() const override {
        return enabler ? enabler() : true;
    }

    virtual void set(const Value& value) override {
        ref = TValue::fromValue(value.get<EnumWrapper>().value);
    }

    virtual Value get() const override {
        return EnumWrapper(TEnum(ref.value()));
    }

    virtual Type getType() const override {
        return Type::FLAGS;
    }

    virtual std::string getName() const override {
        return name;
    }

    virtual std::string getTooltip() const override {
        return tooltip;
    }
};

} // namespace Detail

// connects reference
template <typename TValue>
inline VirtualSettings::Category& VirtualSettings::Category::connect(const std::string& name,
    const std::string& key,
    TValue& value,
    const Float mult,
    const std::string& tooltip) {
    return this->connect(name, key, value, mult, nullptr, tooltip);
    return *this;
}

template <typename TValue>
inline VirtualSettings::Category& VirtualSettings::Category::connect(const std::string& name,
    const std::string& key,
    TValue& value,
    Function<bool()> enabler,
    const std::string& tooltip) {
    return this->connect(name, key, value, 1.f, enabler, tooltip);
}

template <typename TValue>
inline VirtualSettings::Category& VirtualSettings::Category::connect(const std::string& name,
    const std::string& key,
    TValue& value,
    const Float mult,
    Function<bool()> enabler,
    const std::string& tooltip) {
    entries.insert(key, makeAuto<Detail::ValueEntry<TValue>>(value, name, mult, enabler, tooltip));
    return *this;
}

template <typename TValue>
inline VirtualSettings::Category& VirtualSettings::Category::connect(const std::string& name,
    const std::string& key,
    TValue& value,
    const std::string& tooltip) {
    return this->connect(name, key, value, 1.f, nullptr, tooltip);
}

namespace Detail {

/// Partially implements IVirtualEntry
class SettingsEntryBase : public IVirtualEntry {
private:
    std::string name;
    std::string tooltip;
    Function<bool()> enabler;

public:
    SettingsEntryBase(const std::string& name, const std::string& tooltip, Function<bool()> enabler)
        : name(name)
        , tooltip(tooltip)
        , enabler(enabler) {}

    virtual bool enabled() const override {
        return enabler ? enabler() : true;
    }

    virtual std::string getName() const override {
        return name;
    }

    virtual std::string getTooltip() const override {
        return tooltip;
    }
};

template <typename TValue, typename TEnum, typename TEnabler = void>
class SettingsEntry : public SettingsEntryBase {
private:
    Settings<TEnum>& settings;
    TEnum id;

    UnitAdapter<TValue> units;

public:
    SettingsEntry(Settings<TEnum>& settings,
        const TEnum id,
        const std::string& name,
        const Float mult,
        Function<bool()> enabler,
        const std::string& tooltip)
        : SettingsEntryBase(name, tooltip, enabler)
        , settings(settings)
        , id(id)
        , units(mult) {}

    virtual void set(const Value& value) override {
        settings.set(id, units.set(value.get<TValue>()));
    }

    virtual Value get() const override {
        return units.get(settings.template get<TValue>(id));
    }

    virtual Type getType() const override {
        return Type(getTypeIndex<TValue, bool, int, Float, Vector, std::string, Path, EnumWrapper>);
    }
};

/// Partial specialization for Flags
template <typename TValue, typename TEnum>
class SettingsEntry<TValue, TEnum, std::enable_if_t<FlagsTraits<TValue>::isFlags>>
    : public SettingsEntryBase {
private:
    Settings<TEnum>& settings;
    TEnum id;

public:
    SettingsEntry(Settings<TEnum>& settings,
        const TEnum id,
        const std::string& name,
        const Float UNUSED(mult),
        Function<bool()> enabler,
        const std::string& tooltip)
        : SettingsEntryBase(name, tooltip, enabler)
        , settings(settings)
        , id(id) {}

    virtual void set(const Value& value) override {
        settings.set(id, value.get<EnumWrapper>());
    }

    virtual Value get() const override {
        return settings.template get<EnumWrapper>(id);
    }

    virtual Type getType() const override {
        return Type::FLAGS;
    }
};

/// Partial specialization for Path
template <typename TEnum>
class SettingsEntry<Path, TEnum> : public SettingsEntryBase {
private:
    Settings<TEnum>& settings;
    TEnum id;

public:
    SettingsEntry(Settings<TEnum>& settings,
        const TEnum id,
        const std::string& name,
        const Float UNUSED(mult),
        Function<bool()> enabler,
        const std::string& tooltip)
        : SettingsEntryBase(name, tooltip, enabler)
        , settings(settings)
        , id(id) {}

    virtual void set(const Value& value) override {
        settings.set(id, value.get<Path>().native());
    }

    virtual Value get() const override {
        return Path(settings.template get<std::string>(id));
    }

    virtual Type getType() const override {
        return Type::PATH;
    }
};


} // namespace Detail

template <typename TValue, typename TEnum>
VirtualSettings::Category& VirtualSettings::Category::connect(const std::string& name,
    Settings<TEnum>& settings,
    const TEnum id,
    Function<bool()> enabler,
    const Float mult,
    const std::string& tooltip) {

    const std::string key = Settings<TEnum>::getEntryName(id);
    AutoPtr<IVirtualEntry> entry =
        makeAuto<Detail::SettingsEntry<TValue, TEnum>>(settings, id, name, mult, enabler, tooltip);
    entries.insert(key, std::move(entry));

    return *this;
}

template <typename TValue, typename TEnum>
VirtualSettings::Category& VirtualSettings::Category::connect(const std::string& name,
    Settings<TEnum>& settings,
    const TEnum id,
    Function<bool()> enabler,
    const Float mult) {
    return this->connect<TValue>(name, settings, id, enabler, mult, "");
}

template <typename TValue, typename TEnum>
VirtualSettings::Category& VirtualSettings::Category::connect(const std::string& name,
    Settings<TEnum>& settings,
    const TEnum id,
    const Float mult) {
    return this->connect<TValue>(name, settings, id, nullptr, mult, "");
}

template <typename TValue, typename TEnum>
VirtualSettings::Category& VirtualSettings::Category::connect(const std::string& name,
    Settings<TEnum>& settings,
    const TEnum id,
    const std::string& tooltip) {
    return this->connect<TValue>(name, settings, id, nullptr, 1.f, tooltip);
}

NAMESPACE_SPH_END
