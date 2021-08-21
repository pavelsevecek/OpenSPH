#include "run/VirtualSettings.h"

NAMESPACE_SPH_BEGIN

namespace Detail {

template <typename T, typename = void>
class UnitAdapter {
public:
    INLINE const T& get(const T& input, const Float mult) const {
        SPH_ASSERT(mult == 1._f, "Units not implemented for entries other than float or vector");
        return input;
    }

    INLINE const T& set(const T& input, const Float mult) const {
        SPH_ASSERT(mult == 1._f, "Units not implemented for entries other than float or vector");
        return input;
    }
};

template <typename T>
class UnitAdapter<T, std::enable_if_t<std::is_same<T, Float>::value || std::is_same<T, Vector>::value>> {
public:
    INLINE T get(const T& input, const Float mult) const {
        return input / mult;
    }

    INLINE T set(const T& input, const Float mult) const {
        return input * mult;
    }
};

} // namespace Detail

inline void VirtualSettings::Category::addEntry(const String& key, AutoPtr<IVirtualEntry>&& entry) {
    entries.insert(key, std::move(entry));
}

namespace Detail {

template <typename TValue, typename = void>
class ValueEntry : public EntryControl {
private:
    TValue& ref;
    String name;

public:
    ValueEntry(TValue& ref, const String& name)
        : ref(ref)
        , name(name) {}

    virtual void setImpl(const Value& value) override {
        UnitAdapter<TValue> adapter;
        ref = adapter.set(value.get<TValue>(), mult);
    }

    virtual Value get() const override {
        UnitAdapter<TValue> adapter;
        return adapter.get(ref, mult);
    }

    virtual Type getType() const override {
        // path has to be included to get correct index of EnumWrapper
        return Type(
            getTypeIndex<TValue, bool, int, Float, Vector, Interval, String, Path, EnumWrapper, ExtraEntry>);
    }

    virtual String getName() const override {
        return name;
    }
};

template <typename TValue>
class ValueEntry<TValue, std::enable_if_t<FlagsTraits<TValue>::isFlags>> : public EntryControl {
private:
    TValue& ref;
    String name;

    using TEnum = typename FlagsTraits<TValue>::Type;

public:
    ValueEntry(TValue& ref, const String& name)
        : ref(ref)
        , name(name) {}

    virtual void setImpl(const Value& value) override {
        ref = TValue::fromValue(value.get<EnumWrapper>().value);
    }

    virtual Value get() const override {
        return EnumWrapper(TEnum(ref.value()));
    }

    virtual Type getType() const override {
        return Type::FLAGS;
    }

    virtual String getName() const override {
        return name;
    }
};

} // namespace Detail

template <typename TValue>
inline EntryControl& VirtualSettings::Category::connect(const String& name,
    const String& key,
    TValue& value) {
    auto entry = makeAuto<Detail::ValueEntry<TValue>>(value, name);
    EntryControl& control = *entry;
    entries.insert(key, std::move(entry));
    return control;
}


namespace Detail {

template <typename TEnum>
inline String makeTooltip(const TEnum id) {
    Optional<String> key = Settings<TEnum>::getEntryName(id);
    Optional<int> type = Settings<TEnum>::getEntryType(id);
    String scriptTooltip;
    if (key) {
        String typeName = Settings<TEnum>::typeToString(type.value());
        scriptTooltip = "Script name: " + key.value() + " (" + typeName + ")";
    }

    Optional<String> desc = Settings<TEnum>::getEntryDesc(id);
    if (desc) {
        String tooltip = desc.value();
        if (key) {
            tooltip += "\n\n" + scriptTooltip;
        }
        return tooltip;
    } else {
        return scriptTooltip;
    }
}

template <typename TValue, typename TEnum, typename TEnabler = void>
class SettingsEntry : public EntryControl {
private:
    Settings<TEnum>& settings;
    String name;
    TEnum id;

public:
    SettingsEntry(Settings<TEnum>& settings, const TEnum id, const String& name)
        : settings(settings)
        , name(name)
        , id(id) {
        tooltip = makeTooltip(id);
    }

    virtual void setImpl(const Value& value) override {
        UnitAdapter<TValue> adapter;
        settings.set(id, adapter.set(value.get<TValue>(), mult));
    }

    virtual Value get() const override {
        UnitAdapter<TValue> adapter;
        return adapter.get(settings.template get<TValue>(id), mult);
    }

    virtual Type getType() const override {
        return Type(getTypeIndex<TValue, bool, int, Float, Vector, Interval, String, Path, EnumWrapper>);
    }

    virtual String getName() const override {
        return name;
    }
};

/// Partial specialization for Flags
template <typename TValue, typename TEnum>
class SettingsEntry<TValue, TEnum, std::enable_if_t<FlagsTraits<TValue>::isFlags>> : public EntryControl {
private:
    Settings<TEnum>& settings;
    String name;
    TEnum id;

public:
    SettingsEntry(Settings<TEnum>& settings, const TEnum id, const String& name)
        : settings(settings)
        , name(name)
        , id(id) {
        tooltip = makeTooltip(id);
    }

    virtual void setImpl(const Value& value) override {
        settings.set(id, value.get<EnumWrapper>());
    }

    virtual Value get() const override {
        return settings.template get<EnumWrapper>(id);
    }

    virtual Type getType() const override {
        return Type::FLAGS;
    }

    virtual String getName() const override {
        return name;
    }
};

/// Partial specialization for Path
template <typename TEnum>
class SettingsEntry<Path, TEnum> : public EntryControl {
private:
    Settings<TEnum>& settings;
    String name;
    TEnum id;

public:
    SettingsEntry(Settings<TEnum>& settings, const TEnum id, const String& name)
        : settings(settings)
        , name(name)
        , id(id) {
        tooltip = makeTooltip(id);
    }

    virtual void setImpl(const Value& value) override {
        settings.set(id, value.get<Path>().string());
    }

    virtual Value get() const override {
        return Path(settings.template get<String>(id));
    }

    virtual Type getType() const override {
        return Type::PATH;
    }

    virtual String getName() const override {
        return name;
    }
};


} // namespace Detail

template <typename TValue, typename TEnum>
EntryControl& VirtualSettings::Category::connect(const String& name,
    Settings<TEnum>& settings,
    const TEnum id) {
    const Optional<String> key = Settings<TEnum>::getEntryName(id);
    if (!key) {
        throw InvalidSetup("No settings entry with id {}", int(id));
    }
    auto entry = makeAuto<Detail::SettingsEntry<TValue, TEnum>>(settings, id, name);
    EntryControl& control = *entry;
    entries.insert(key.value(), std::move(entry));
    return control;
}


template <typename TEnum, typename>
void VirtualSettings::set(const TEnum id, const IVirtualEntry::Value& value) {
    const Optional<String> key = Settings<TEnum>::getEntryName(id);
    if (!key) {
        throw InvalidSetup("No entry with ID {}", int(id));
    }

    this->set(key.value(), value);
}

NAMESPACE_SPH_END
