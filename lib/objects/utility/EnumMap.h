#pragma once

/// \file EnumMap.h
/// \brief Helper object for converting enums to string, listing all available values of enum, etc.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz
/// \date 2016-2018

#include "objects/containers/FlatMap.h"
#include <typeindex>

NAMESPACE_SPH_BEGIN

struct EnumValue {
    /// Text of the value
    std::string value;

    /// Description
    std::string desc;
};


using EnumRecord = FlatMap<int, EnumValue>;

template <typename TEnum>
struct EnumInputValue {
    TEnum id;
    std::string value;
    std::string desc;
};

class EnumMap {
private:
    FlatMap<std::size_t, EnumRecord> records;

public:
    template <typename TEnum>
    static EnumMap& addEnum(Array<EnumInputValue<TEnum>>&& input) {
        EnumMap& instance = getInstance();
        // convert enum values to ints to get rid of the type
        FlatMap<int, EnumValue> map;
        for (auto value : input) {
            map.insert(int(value.id), EnumValue{ value.value, value.desc });
        }
        const std::size_t id = typeid(TEnum).hash_code();
        // ASSERT(!instance.records.contains(id));
        instance.records.insert(id, std::move(map));
        return instance;
    }

    static std::string toString(const int value, const std::size_t id) {
        EnumMap& instance = getInstance();
        Optional<EnumRecord&> record = instance.records.tryGet(id);
        ASSERT(record);
        if (Optional<EnumValue&> e = record->tryGet(value)) {
            // this is one of the enum values, return the text value
            return e->value;
        } else {
            // the value is not directly in the enum, but can be composed of flags
            std::string result;
            for (int i = 1; i <= value; i *= 2) {
                if ((value & i) == 0) {
                    continue;
                }
                Optional<EnumValue&> pair = record->tryGet(i);
                ASSERT(pair, i, value);
                if (!result.empty()) {
                    result += " | ";
                }
                result += pair->value;
            }
            return result;
        }
    }

    static Optional<std::size_t> fromString(const std::string& value, const std::size_t id) {
        EnumMap& instance = getInstance();
        Optional<EnumRecord&> record = instance.records.tryGet(id);
        ASSERT(record);
        for (auto pair : record.value()) {
            if (pair.value.value == value) { // erm ...
                return pair.key;
            }
        }
        return NOTHING;
    }

    template <typename TEnum>
    static std::string getDesc() {
        EnumMap& instance = getInstance();
        const std::size_t id = typeid(TEnum).hash_code();
        Optional<EnumRecord&> record = instance.records.tryGet(id);
        ASSERT(record);
        std::string desc;
        Size idx = 0;
        for (auto pair : record.value()) {
            if (idx > 0) {
                desc += "\n";
            }
            desc += " - " + pair.value.value + ": " + pair.value.desc;
            ++idx;
        }
        return desc;
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
