#include "quantities/Storage.h"
#include "physics/Eos.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

Storage::Storage() = default;

Storage::~Storage() = default;

Storage::Storage(const BodySettings& settings) {
    materials.push(Material(settings));
}

Storage::Storage(Storage&& other)
    : quantities(std::move(other.quantities))
    , materials(std::move(other.materials)) {}

Storage& Storage::operator=(Storage&& other) {
    quantities = std::move(other.quantities);
    materials = std::move(other.materials);
    return *this;
}

Material& Storage::getMaterial(const Size particleIdx) {
    ASSERT(!materials.empty());
    /// \todo profile and possibly optimize (cache matIdxs array)
    Array<Size>& matIdxs = this->getValue<Size>(QuantityKey::MATERIAL_IDX);
    return materials[matIdxs[particleIdx]];
}

void Storage::merge(Storage&& other) {
    // must contain the same quantities
    ASSERT(this->getQuantityCnt() == other.getQuantityCnt());
    // as material id is an index to array, we have to increase indices before the merge
    if (this->has(QuantityKey::MATERIAL_IDX)) {
        ASSERT(other.has(QuantityKey::MATERIAL_IDX));
        Array<Size>& matIdxs = other.getValue<Size>(QuantityKey::MATERIAL_IDX);
        for (Size& id : matIdxs) {
            id += this->materials.size();
        }
        materials.pushAll(std::move(other.materials));
    }

    // merge all quantities
    iteratePair<VisitorEnum::ALL_BUFFERS>(
        quantities, other.quantities, [](auto&& ar1, auto&& ar2) { ar1.pushAll(std::move(ar2)); });
}

void Storage::init() {
    iterate<VisitorEnum::HIGHEST_DERIVATIVES>(quantities, [](auto&& dv) {
        using TValue = typename std::decay_t<decltype(dv)>::Type;
        dv.fill(TValue(0._f));
    });
}

Storage Storage::clone(const Flags<VisitorEnum> flags) const {
    Storage cloned;
    for (const auto& q : quantities) {
        cloned.quantities[q.first] = q.second.clone(flags);
    }
    return cloned;
}

void Storage::swap(Storage& other, const Flags<VisitorEnum> flags) {
    ASSERT(this->getQuantityCnt() == other.getQuantityCnt());
    for (auto i1 = quantities.begin(), i2 = other.quantities.begin(); i1 != quantities.end(); ++i1, ++i2) {
        i1->second.swap(i2->second, flags);
    }
}


NAMESPACE_SPH_END
