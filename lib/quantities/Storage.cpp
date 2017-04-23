#include "quantities/Storage.h"
#include "physics/Eos.h"
#include "quantities/AbstractMaterial.h"
#include "quantities/Iterate.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN


StorageSequence::StorageSequence(Storage& storage)
    : storage(storage) {}

StorageIterator StorageSequence::begin() {
    return storage.quantities.begin();
}

StorageIterator StorageSequence::end() {
    return storage.quantities.end();
}

Size StorageSequence::size() const {
    return storage.getQuantityCnt();
}


Storage::Storage() = default;

Storage::Storage(std::unique_ptr<Abstract::Material>&& material) {
    materials.push(std::move(material));
}

Storage::~Storage() = default;

Storage::Storage(Storage&& other)
    : quantities(std::move(other.quantities))
    , materials(std::move(other.materials)) {}

Storage& Storage::operator=(Storage&& other) {
    quantities = std::move(other.quantities);
    materials = std::move(other.materials);
    return *this;
}

MaterialView Storage::getMaterial(const Size matId) {
    ASSERT(this->has(QuantityId::MATERIAL_IDX));
    return MaterialView(materials[matId].get(), this->getValue<Size>(QuantityId::MATERIAL_IDX), matId);
}

MaterialView Storage::getMaterialOfParticle(const Size particleIdx) {
    ASSERT(this->has(QuantityId::MATERIAL_IDX));
    ArrayView<Size> matIdxs = this->getValue<Size>(QuantityId::MATERIAL_IDX);
    return this->getMaterial(matIdxs[particleIdx]);
}

Range Storage::getRange(const QuantityId id, const Size matIdx) const {
    return materials[matIdx]->range(id);
}

StorageSequence Storage::getQuantities() {
    return *this;
}

Size Storage::getMaterialCnt() const {
    return materials.size();
}

Size Storage::getQuantityCnt() const {
    return quantities.size();
}

Size Storage::getParticleCnt() const {
    if (quantities.empty()) {
        return 0;
    } else {
        return quantities.begin()->second.size();
    }
}

void Storage::merge(Storage&& other) {
    // must contain the same quantities
    ASSERT(this->getQuantityCnt() == other.getQuantityCnt());
    // as material id is an index to array, we have to increase indices before the merge
    if (this->has(QuantityId::MATERIAL_IDX)) {
        ASSERT(other.has(QuantityId::MATERIAL_IDX));
        Array<Size>& matIdxs = other.getValue<Size>(QuantityId::MATERIAL_IDX);
        for (Size& id : matIdxs) {
            id += this->materials.size();
        }
        materials.pushAll(std::move(other.materials));
    }

    // merge all quantities
    iteratePair<VisitorEnum::ALL_BUFFERS>(
        *this, other, [](auto&& ar1, auto&& ar2) { ar1.pushAll(std::move(ar2)); });
}

void Storage::init() {
    iterate<VisitorEnum::HIGHEST_DERIVATIVES>(*this, [](const QuantityId, auto&& dv) {
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

void Storage::resize(const Size newParticleCnt) {
    ASSERT(getQuantityCnt() > 0);
    iterate<VisitorEnum::ALL_BUFFERS>(
        *this, [newParticleCnt](auto&& buffer) { buffer.resize(newParticleCnt); });
}

void Storage::swap(Storage& other, const Flags<VisitorEnum> flags) {
    ASSERT(this->getQuantityCnt() == other.getQuantityCnt());
    for (auto i1 = quantities.begin(), i2 = other.quantities.begin(); i1 != quantities.end(); ++i1, ++i2) {
        i1->second.swap(i2->second, flags);
    }
}

bool Storage::isValid() const {
    const Size cnt = this->getParticleCnt();
    bool valid = true;
    iterate<VisitorEnum::ALL_BUFFERS>(const_cast<Storage&>(*this), [cnt, &valid](auto&& buffer) {
        if (buffer.size() != cnt) {
            valid = false;
        }
    });
    return valid;
}

void Storage::removeAll() {
    quantities.clear();
    materials.clear();
}

NAMESPACE_SPH_END
