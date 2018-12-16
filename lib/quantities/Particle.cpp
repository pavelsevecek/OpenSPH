#include "quantities/Particle.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

struct ParticleVisitor {
    FlatMap<QuantityId, Particle::InternalQuantityData>& data;

    template <typename TValue>
    void visit(const QuantityId id, const Quantity& q, const Size idx) {
        const auto& values = q.getAll<TValue>();
        switch (q.getOrderEnum()) {
        case OrderEnum::SECOND:
            data[id].d2t = values[2][idx];
            SPH_FALLTHROUGH
        case OrderEnum::FIRST:
            data[id].dt = values[1][idx];
            SPH_FALLTHROUGH
        case OrderEnum::ZERO:
            data[id].value = values[0][idx];
            break;
        default:
            NOT_IMPLEMENTED;
        }
    }
};


Particle::Particle(const Storage& storage, const Size idx)
    : idx(idx) {
    for (ConstStorageElement i : storage.getQuantities()) {
        data.insert(i.id, InternalQuantityData{});

        ParticleVisitor visitor{ data };
        dispatch(i.quantity.getValueEnum(), visitor, i.id, i.quantity, idx);
    }
}

Particle::Particle(const QuantityId id, const Dynamic& value, const Size idx)
    : idx(idx) {
    data.insert(id, InternalQuantityData{});
    data[id].value = value;
}

Particle::Particle(const Particle& other) {
    *this = other;
}

Particle::Particle(Particle&& other) {
    *this = std::move(other);
}

Particle& Particle::operator=(const Particle& other) {
    data = other.data.clone();
    idx = other.idx;
    return *this;
}

Particle& Particle::operator=(Particle&& other) {
    data = std::move(other.data);
    idx = other.idx;
    return *this;
}

Particle& Particle::addValue(const QuantityId id, const Dynamic& value) {
    if (!data.contains(id)) {
        data.insert(id, InternalQuantityData{});
    }
    data[id].value = value;
    return *this;
}

Particle& Particle::addDt(const QuantityId id, const Dynamic& value) {
    if (!data.contains(id)) {
        data.insert(id, InternalQuantityData{});
    }
    data[id].dt = value;
    return *this;
}

Particle& Particle::addD2t(const QuantityId id, const Dynamic& value) {
    if (!data.contains(id)) {
        data.insert(id, InternalQuantityData{});
    }
    data[id].d2t = value;
    return *this;
}

Dynamic Particle::getValue(const QuantityId id) const {
    Optional<const InternalQuantityData&> quantity = data.tryGet(id);
    ASSERT(quantity);
    return quantity->value;
}

Dynamic Particle::getDt(const QuantityId id) const {
    Optional<const InternalQuantityData&> quantity = data.tryGet(id);
    ASSERT(quantity);
    return quantity->dt;
}

Dynamic Particle::getD2t(const QuantityId id) const {
    Optional<const InternalQuantityData&> quantity = data.tryGet(id);
    ASSERT(quantity);
    return quantity->d2t;
}

Particle::ValueIterator::ValueIterator(const ActIterator iterator)
    : iter(iterator) {}

Particle::ValueIterator& Particle::ValueIterator::operator++() {
    ++iter;
    return *this;
}

Particle::QuantityData Particle::ValueIterator::operator*() const {
    const InternalQuantityData& internal = iter->value;
    DynamicId type;
    if (internal.value) {
        type = internal.value.getType();
        ASSERT(internal.dt.empty() || internal.dt.getType() == type);
        ASSERT(internal.d2t.empty() || internal.d2t.getType() == type);
    } else if (internal.dt) {
        type = internal.dt.getType();
        ASSERT(internal.d2t.empty() || internal.d2t.getType() == type);
    } else {
        ASSERT(internal.d2t);
        type = internal.d2t.getType();
    }
    return { iter->key, type, internal.value, internal.dt, internal.d2t };
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
