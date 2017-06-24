#include "quantities/Particle.h"

NAMESPACE_SPH_BEGIN

struct Particle::Visitor {
    std::map<QuantityId, Particle::InternalQuantityData>& data;

    template <typename TValue>
    void visit(const QuantityId id, const Quantity& q, const Size idx) {
        const auto& values = q.getAll<TValue>();
        data[id].value = values[0][idx];
        data[id].dt = values[1][idx];
        data[id].d2t = values[2][idx];
    }
};

Particle::Particle(const Storage& storage, const Size idx)
    : idx(idx) {
    for (ConstStorageElement i : storage.getQuantities()) {
        Visitor visitor{ data };
        dispatch(i.quantity.getValueEnum(), visitor, i.id, i.quantity, idx);
    }
}

Particle::Particle(const QuantityId id, const Value& value, const Size idx)
    : idx(idx) {
    data[id].value = value;
}

Particle& Particle::addValue(const QuantityId id, const Value& value) {
    data[id].value = value;
    return *this;
}

Particle& Particle::addDt(const QuantityId id, const Value& value) {
    data[id].dt = value;
    return *this;
}

Particle& Particle::addD2t(const QuantityId id, const Value& value) {
    data[id].d2t = value;
    return *this;
}

Value Particle::getValue(const QuantityId id) const {
    auto iter = data.find(id);
    ASSERT(iter != data.end());
    return iter->second.value;
}

Value Particle::getDt(const QuantityId id) const {
    auto iter = data.find(id);
    ASSERT(iter != data.end());
    return iter->second.dt;
}

Value Particle::getD2t(const QuantityId id) const {
    auto iter = data.find(id);
    ASSERT(iter != data.end());
    return iter->second.d2t;
}

Particle::ValueIterator::ValueIterator(const Iterator iterator)
    : iter(iterator) {}

Particle::ValueIterator& Particle::ValueIterator::operator++() {
    ++iter;
    return *this;
}

Particle::QuantityData Particle::ValueIterator::operator*() const {
    const InternalQuantityData& internal = iter->second;
    ValueId type;
    if (internal.value) {
        type = internal.value.getType();
        ASSERT(internal.dt.empty() || internal.dt.getType() == type);
        ASSERT(internal.d2t.empty() || internal.d2t.getType() == type);
    } else if (internal.dt) {
        type = internal.value.getType();
        ASSERT(internal.d2t.empty() || internal.d2t.getType() == type);
    } else {
        ASSERT(internal.d2t);
        type = internal.d2t.getType();
    }
    return { iter->first, type, internal.value, internal.dt, internal.d2t };
}

bool Particle::ValueIterator::operator!=(const ValueIterator& other) const {
    return iter != other.iter;
}

Particle::ValueIterator Particle::begin() const {
    return ValueIterator(data.begin());
}

Particle::ValueIterator Particle::end() const {
    return ValueIterator(data.end());
}

NAMESPACE_SPH_END
