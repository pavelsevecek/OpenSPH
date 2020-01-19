#pragma once

/// \file VirtualSettings.h
/// \brief Object providing connection between component parameters and values exposed to the user
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "io/Path.h"
#include "objects/containers/UnorderedMap.h"
#include "objects/wrappers/Function.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

/// \brief Provides an interface for implementing new types of entries.
///
/// Class \ref IVirtualEntry can store basic types, such as bool, int, std::string, etc. When necessary to
/// store more complex data, the user can implement \ref IExtraEntry interface and store the data using \ref
/// ExtraEntry wrapper.
class IExtraEntry : public Polymorphic {
public:
    virtual std::string toString() const = 0;

    virtual void fromString(const std::string& s) = 0;

    /// \todo use ClonePtr instead
    virtual AutoPtr<IExtraEntry> clone() const = 0;
};

/// \brief Copyable wrapper of a \ref IExtraEntry.
class ExtraEntry {
public:
    AutoPtr<IExtraEntry> entry;

public:
    explicit ExtraEntry(AutoPtr<IExtraEntry>&& entry)
        : entry(std::move(entry)) {}

    ExtraEntry(const ExtraEntry& other)
        : entry(other.entry->clone()) {}

    ExtraEntry& operator=(const ExtraEntry& other) {
        entry = other.entry->clone();
        return *this;
    }

    std::string toString() const {
        return entry->toString();
    }

    void fromString(const std::string& s) const {
        entry->fromString(s);
    }

    RawPtr<IExtraEntry> getEntry() const {
        return entry.get();
    }
};

/// \brief Represents a virtual entry in the settings.
///
/// This entry may be connected to a single reference, an entry in \ref Settings object or other user-defined
/// value. It provides an abstraction through which state of workers can be queried and modified.
class IVirtualEntry : public Polymorphic {
public:
    enum class Type { BOOL, INT, FLOAT, VECTOR, INTERVAL, STRING, PATH, ENUM, EXTRA, FLAGS };

    using Value = Variant<bool, int, Float, Vector, Interval, std::string, Path, EnumWrapper, ExtraEntry>;

    /// \brief Returns if this entry is currently enabled.
    ///
    /// Implementation may return false if the entry does not have any effect for current setup of the worker,
    /// for example if the entry is associated with a parameter of a solver which is not being used.
    virtual bool enabled() const = 0;

    /// \brief Modifies the current value of the entry.
    ///
    /// Function shall throw \ref Exception (or derived type) if the type of the value differs from type of
    /// the entry.
    virtual void set(const Value& value) = 0;

    /// \brief Returns the currently stored value.
    virtual Value get() const = 0;

    /// \brief Returns the type of this entry.
    virtual Type getType() const = 0;

    /// \brief Returns a descriptive name of the entry.
    ///
    /// This name is intended to be presented to the user (in UI, printed in log, etc.). It may be the same as
    /// the key associated with an entry, although the key is only used as an unique identifier and does not
    /// have to be human-readable.
    virtual std::string getName() const = 0;

    /// \brief Returns an optional description of the entry.
    virtual std::string getTooltip() const {
        return "";
    }

    /// \brief Returns true if the entry can modify multiple values simultaneously.
    ///
    /// Usually, there is a 1-1 correspondence between entries and values (stored either in \ref Settings or
    /// directly in the worker). However, an entry can modify other values as well, in which case it should
    /// signal this by returning true from this function.
    virtual bool hasSideEffect() const {
        return false;
    }
};

/// \brief Helper object, allowing to add units, tooltips and additional properties into the entry created
/// with \ref VirtualSettings::Category::connect.
///
/// Each member function returns a reference to the object in order to allow queuing the calls, such as
/// \code
/// control.setTooltip("This is a tooltip").setUnits(1.e3f);
/// \endcode
///
/// It partially implements the \ref IVirtualEntry interface to avoid code duplication.
class EntryControl : public IVirtualEntry {
protected:
    std::string tooltip;
    Float mult = 1._f;
    Function<bool()> enabler = nullptr;
    Function<void(const Value& newValue)> accessor = nullptr;

public:
    /// \brief Adds or replaces the previous tooltip associanted with the entry.
    EntryControl& setTooltip(const std::string& newTooltip);

    /// \brief Sets units in which the entry is stored.
    ///
    /// Note that the units are currently only applied for \ref Float or \ref Vector entries. Other entries
    /// ignore the value.
    EntryControl& setUnits(const Float newMult);

    /// \brief Adds or replaces the enabler functor of the entry.
    ///
    /// Enabler specifies whether the entry is accessible or not. The user can still call the member function
    /// \ref set or \ref get of the entry even if the functor returns false, but it indicates that the entry
    /// has no meaning in the context of the current settings. For example, enabler should returns false for
    /// entries associated with parameters of the boundary if there are no boundary conditions in the
    /// simulations.
    EntryControl& setEnabler(Function<bool()> newEnabler);

    /// \brief Adds or replaces the previous functor called when the entry changes, i.e. when \ref set
    /// function is called.
    EntryControl& setAccessor(Function<void(const Value& newValue)> newAccessor);

protected:
    virtual bool enabled() const override;

    virtual std::string getTooltip() const override;

    virtual bool hasSideEffect() const override;
};

/// \brief Holds a map of virtual entries, associated with a unique name.
///
/// The key-value pairs are not stored in the settings directly, they are stored in settings categories; the
/// \ref VirtualSettings object then holds a map of categories. This provides clustering of related entries,
/// which helps to separate the entries in UI.
class VirtualSettings {
public:
    class Category {
        friend class VirtualSettings;

    private:
        UnorderedMap<std::string, AutoPtr<IVirtualEntry>> entries;

    public:
        /// \brief Manually adds a new entry into the settings.
        ///
        /// Mostly intended for custom implementations of \ref IVirtualEntry. For most uses, using \ref
        /// connect is more conventient way to connect values with \ref IVirtualSettings.
        void addEntry(const std::string& key, AutoPtr<IVirtualEntry>&& entry);

        /// \brief Connects to given reference.
        template <typename TValue>
        EntryControl& connect(const std::string& name, const std::string& key, TValue& value);

        /// \brief Connects to value in \ref Settings object
        template <typename TValue, typename TEnum>
        EntryControl& connect(const std::string& name, Settings<TEnum>& settings, const TEnum id);
    };


private:
    UnorderedMap<std::string, Category> categories;

public:
    /// \brief Modifies an existing entry in the settings.
    ///
    /// This function cannot be used to add a new entry. Use \ref Category to create entries.
    /// \param key Identifier of the entry
    /// \param value New value of the entry
    /// \throw InvalidSetup if no entry with given key exists.
    void set(const std::string& key, const IVirtualEntry::Value& value);

    /// \brief Overload allowing to use an ID associated with a \ref Settings entry.
    ///
    /// This is useful if the virtual entry is connected to an entry in a \ref Settings object. This way, an
    /// enum value can be used instead of string, ensuring the value is valid (string may contain typos) and
    /// can be easily renamed if necessary.
    ///
    /// Note that there is no check that the virtual entry is really connected to a "real" entry with given
    /// ID, virtual entry is (intentionally) a black box here.
    /// \param id ID of the connected entry
    /// \param value New value of the entry
    /// \throw InvalidSetup if the ID is invalid or no entry with given ID exists.
    template <typename TEnum, typename = std::enable_if_t<std::is_enum<TEnum>::value>>
    void set(const TEnum id, const IVirtualEntry::Value& value);

    /// \brief Returns a value of an entry.
    ///
    /// \param key Identifier of the entry
    /// \throw InvalidSetup if no entry with given key exists.
    IVirtualEntry::Value get(const std::string& key) const;

    /// \brief Creates a new category of entries.
    ///
    /// Returned object can be used to add entries into settings.
    /// \param name Name of the created category.
    Category& addCategory(const std::string& name);

    /// \brief Interface allowing to enumerate all entries in the settings.
    class IEntryProc : public Polymorphic {
    public:
        /// \brief Called for every category in the settings.
        ///
        /// Every subsequent call of \ref onEntry corresponds to an entry belonging to this category.
        /// \param name Name of the category
        virtual void onCategory(const std::string& name) const = 0;

        /// \brief Called for every entry in the current category.
        ///
        /// \param key Identifier of the entry
        /// \param entry Stored entry
        virtual void onEntry(const std::string& key, IVirtualEntry& entry) const = 0;
    };

    /// \brief Enumerates all entries stored in the settings.
    void enumerate(const IEntryProc& proc);
};

NAMESPACE_SPH_END

#include "run/VirtualSettings.inl.h"
