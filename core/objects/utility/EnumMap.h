#pragma once

/// \file EnumMap.h
/// \brief Helper object for converting enums to string, listing all available values of enum, etc.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz
/// \date 2016-2021

#include "objects/containers/FlatMap.h"
#include "objects/containers/String.h"
#include "objects/containers/UnorderedMap.h"
#include <typeindex>

NAMESPACE_SPH_BEGIN

struct EnumValue {
    /// Text of the value
    String value;

    /// Description
    String desc;
};

/// Maps a numerical value to a string value and description.
using EnumRecord = UnorderedMap<int, EnumValue>;

/// Unique identifier of an enum type.
///
/// Made optional to be default-constructible and usable in FlatMap.
using EnumIndex = Optional<std::type_index>;

template <typename TEnum>
struct EnumInputValue {
    TEnum id;
    String value;
    String desc;
};

class EnumMap {
private:
    struct CompareEnums {
        INLINE bool operator()(const EnumIndex& key1, const EnumIndex& key2) const {
            if (key1 && key2) {
                return key1.value() < key2.value();
            } else if (key2) {
                return true;
            } else {
                return false;
            }
        }
    };

    FlatMap<EnumIndex, EnumRecord, CompareEnums> records;

public:
    template <typename TEnum>
    static EnumMap& addEnum(Array<EnumInputValue<TEnum>>&& input) {
        EnumMap& instance = getInstance();
        // convert enum values to ints to get rid of the type
        UnorderedMap<int, EnumValue> map;
        for (auto value : input) {
            map.insert(int(value.id), EnumValue{ value.value, value.desc });
        }
        const EnumIndex index = std::type_index(typeid(TEnum));
        instance.records.insert(index, std::move(map));
        return instance;
    }

    template <typename TEnum>
    static String toString(const TEnum value) {
        return toString(int(value), std::type_index(typeid(TEnum)));
    }

    static String toString(const int value, const EnumIndex& index) {
        EnumMap& instance = getInstance();
        Optional<EnumRecord&> record = instance.records.tryGet(index);
        SPH_ASSERT(record);
        if (Optional<EnumValue&> e = record->tryGet(value)) {
            // this is one of the enum values, return the text value
            return e->value;
        } else {
            // the value is not directly in the enum, but can be composed of flags
            String result;
            for (int i = 1; i <= value; i *= 2) {
                if ((value & i) == 0) {
                    continue;
                }
                Optional<EnumValue&> pair = record->tryGet(i);
                SPH_ASSERT(pair, i, value);
                if (!result.empty()) {
                    result += " | ";
                }
                result += pair->value;
            }
            if (result.empty()) {
                // empty flags, represent by 0
                result += '0';
            }
            return result;
        }
    }

    template <typename TEnum>
    static Optional<TEnum> fromString(const String& value) {
        Optional<int> id = fromString(value, std::type_index(typeid(TEnum)));
        return optionalCast<TEnum>(id);
    }

    static Optional<int> fromString(const String& value, const EnumIndex& index) {
        EnumMap& instance = getInstance();
        Optional<EnumRecord&> record = instance.records.tryGet(index);
        SPH_ASSERT(record);
        for (auto pair : record.value()) {
            if (pair.value().value == value) { // erm ...
                return pair.key();
            }
        }
        return NOTHING;
    }

    template <typename TEnum>
    static String getDesc() {
        return getDesc(std::type_index(typeid(TEnum)));
    }

    static String getDesc(const EnumIndex& index) {
        EnumMap& instance = getInstance();
        Optional<EnumRecord&> record = instance.records.tryGet(index);
        SPH_ASSERT(record);
        String desc;
        Size idx = 0;
        for (auto pair : record.value()) {
            if (idx > 0) {
                desc += "\n";
            }
            desc += " - " + pair.value().value + ": " + pair.value().desc;
            ++idx;
        }
        return desc;
    }

    template <typename TEnum>
    static Array<TEnum> getAll() {
        EnumMap& instance = getInstance();
        const EnumIndex index = std::type_index(typeid(TEnum));
        Optional<EnumRecord&> record = instance.records.tryGet(index);
        SPH_ASSERT(record);

        Array<TEnum> enums;
        for (auto pair : record.value()) {
            enums.push(TEnum(pair.key()));
        }
        return enums;
    }


    static Array<int> getAll(const EnumIndex& index) {
        EnumMap& instance = getInstance();
        Optional<EnumRecord&> record = instance.records.tryGet(index);
        SPH_ASSERT(record);

        Array<int> enums;
        for (auto pair : record.value()) {
            enums.push(pair.key());
        }
        return enums;
    }

private:
    static EnumMap& getInstance() {
        static EnumMap instance;
        return instance;
    }
};

/// \brief Helper class for adding individual enums to the enum map.
template <typename TEnum>
struct RegisterEnum {
public:
    RegisterEnum(Array<EnumInputValue<TEnum>>&& input) {
        EnumMap::addEnum(std::move(input));
    }
};


NAMESPACE_SPH_END
