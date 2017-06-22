#pragma once

#include "objects/wrappers/Value.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

/// \brief Object extracting information about single particle from the storage
class Particle {
private:
    Size idx;
    std::map<QuantityId, Value> values;

    struct Visitor {
        Particle& that;

        template <typename TValue>
        void visit(const QuantityId id, const Quantity& q, const Size idx) {
            that.values[id] = q.getValue<TValue>()[idx];
        }
    };

public:
    Particle(const Storage& storage, const Size idx)
        : idx(idx) {
        for (ConstStorageElement i : storage.getQuantities()) {
            Visitor visitor{ *this };
            dispatch(i.quantity.getValueEnum(), visitor, i.id, i.quantity, idx);
        }
    }
};

NAMESPACE_SPH_END
