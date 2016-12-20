#pragma once

#include "objects/wrappers/Range.h"
#include "storage/Quantity.h"
#include "system/Settings.h"
#include <map>

NAMESPACE_SPH_BEGIN

/// Table containing info about quantities.
class QuantityMap : public Object {
private:
    struct Info {
        ValueEnum type;
        OrderEnum order;
    };

    std::map<QuantityKey, Info> table;

public:
    QuantityMap() = default;

    Info& operator[](const QuantityKey key) { return table[key]; }

    /// Inserts all values from other map into this one.
    /// \todo add check that we do not override some values
    void add(const QuantityMap& other) { table.insert(other.table.begin(), other.table.end()); }
};


NAMESPACE_SPH_END
