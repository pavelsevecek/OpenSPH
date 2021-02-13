#include "quantities/Storage.h"
#include "objects/Exceptions.h"
#include "physics/Eos.h"
#include "quantities/IMaterial.h"
#include "quantities/Iterate.h"
#include "system/Factory.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

StorageIterator::StorageIterator(const ActIterator iterator)
    : iter(iterator) {}

StorageIterator& StorageIterator::operator++() {
    ++iter;
    return *this;
}

StorageElement StorageIterator::operator*() {
    return { iter->key, iter->value };
}

bool StorageIterator::operator==(const StorageIterator& other) const {
    return iter == other.iter;
}

bool StorageIterator::operator!=(const StorageIterator& other) const {
    return iter != other.iter;
}

ConstStorageIterator::ConstStorageIterator(const ActIterator iterator)
    : iter(iterator) {}

ConstStorageIterator& ConstStorageIterator::operator++() {
    ++iter;
    return *this;
}

ConstStorageElement ConstStorageIterator::operator*() {
    return { iter->key, iter->value };
}

bool ConstStorageIterator::operator==(const ConstStorageIterator& other) const {
    return iter == other.iter;
}

bool ConstStorageIterator::operator!=(const ConstStorageIterator& other) const {
    return iter != other.iter;
}

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

ConstStorageSequence::ConstStorageSequence(const Storage& storage)
    : storage(storage) {}

ConstStorageIterator ConstStorageSequence::begin() {
    return storage.quantities.begin();
}

ConstStorageIterator ConstStorageSequence::end() {
    return storage.quantities.end();
}

Size ConstStorageSequence::size() const {
    return storage.getQuantityCnt();
}

InvalidStorageAccess::InvalidStorageAccess(const QuantityId id)
    : Exception("Invalid storage access to quantity " + getMetadata(id).quantityName) {}

InvalidStorageAccess::InvalidStorageAccess(const std::string& message)
    : Exception("Invalid storage access. " + message) {}

static void checkStorageAccess(const bool result, const QuantityId id) {
    if (!result) {
        throw InvalidStorageAccess(id);
    }
}

static void checkStorageAccess(const bool result, const std::string& message) {
    if (!result) {
        throw InvalidStorageAccess(message);
    }
}

Storage::MatRange::MatRange(const SharedPtr<IMaterial>& material, const Size from, const Size to)
    : material(material)
    , from(from)
    , to(to) {
    ASSERT(from < to || (from == 0 && to == 0));
}

Storage::Storage() = default;

Storage::Storage(const SharedPtr<IMaterial>& material) {
    mats.emplaceBack(material, 0, 0);
}

Storage::~Storage() = default;

Storage::Storage(Storage&& other) {
    *this = std::move(other);
}

Storage& Storage::operator=(Storage&& other) {
    quantities = std::move(other.quantities);
    mats = std::move(other.mats);
    dependent = std::move(other.dependent);
    userData = std::move(other.userData);

    if (this->getParticleCnt() > 0) {
        this->update();
    }
    return *this;
}

bool Storage::has(const QuantityId key) const {
    return quantities.contains(key);
}

template <typename TValue>
bool Storage::has(const QuantityId key, const OrderEnum order) const {
    Optional<const Quantity&> quantity = quantities.tryGet(key);
    if (!quantity) {
        return false;
    }
    return quantity->getOrderEnum() == order && quantity->getValueEnum() == GetValueEnum<TValue>::type;
}

template bool Storage::has<Size>(const QuantityId, const OrderEnum) const;
template bool Storage::has<Float>(const QuantityId, const OrderEnum) const;
template bool Storage::has<Vector>(const QuantityId, const OrderEnum) const;
template bool Storage::has<SymmetricTensor>(const QuantityId, const OrderEnum) const;
template bool Storage::has<TracelessTensor>(const QuantityId, const OrderEnum) const;
template bool Storage::has<Tensor>(const QuantityId, const OrderEnum) const;

Quantity& Storage::getQuantity(const QuantityId key) {
    Optional<Quantity&> quantity = quantities.tryGet(key);
    checkStorageAccess(bool(quantity), key);
    return quantity.value();
}

const Quantity& Storage::getQuantity(const QuantityId key) const {
    Optional<const Quantity&> quantity = quantities.tryGet(key);
    checkStorageAccess(bool(quantity), key);
    return quantity.value();
}

template <typename TValue>
StaticArray<Array<TValue>&, 3> Storage::getAll(const QuantityId key) {
    Quantity& q = this->getQuantity(key);
    checkStorageAccess(q.getValueEnum() == GetValueEnum<TValue>::type, key);
    return q.getAll<TValue>();
}

template StaticArray<Array<Size>&, 3> Storage::getAll(const QuantityId);
template StaticArray<Array<Float>&, 3> Storage::getAll(const QuantityId);
template StaticArray<Array<Vector>&, 3> Storage::getAll(const QuantityId);
template StaticArray<Array<SymmetricTensor>&, 3> Storage::getAll(const QuantityId);
template StaticArray<Array<TracelessTensor>&, 3> Storage::getAll(const QuantityId);
template StaticArray<Array<Tensor>&, 3> Storage::getAll(const QuantityId);

template <typename TValue>
StaticArray<const Array<TValue>&, 3> Storage::getAll(const QuantityId key) const {
    const Quantity& q = this->getQuantity(key);
    checkStorageAccess(q.getValueEnum() == GetValueEnum<TValue>::type, key);
    return q.getAll<TValue>();
}

template StaticArray<const Array<Size>&, 3> Storage::getAll(const QuantityId) const;
template StaticArray<const Array<Float>&, 3> Storage::getAll(const QuantityId) const;
template StaticArray<const Array<Vector>&, 3> Storage::getAll(const QuantityId) const;
template StaticArray<const Array<SymmetricTensor>&, 3> Storage::getAll(const QuantityId) const;
template StaticArray<const Array<TracelessTensor>&, 3> Storage::getAll(const QuantityId) const;
template StaticArray<const Array<Tensor>&, 3> Storage::getAll(const QuantityId) const;

template <typename TValue>
Array<TValue>& Storage::getValue(const QuantityId key) {
    Quantity& q = this->getQuantity(key);
    checkStorageAccess(q.getValueEnum() == GetValueEnum<TValue>::type, key);
    return q.getValue<TValue>();
}

template Array<Size>& Storage::getValue(const QuantityId);
template Array<Float>& Storage::getValue(const QuantityId);
template Array<Vector>& Storage::getValue(const QuantityId);
template Array<SymmetricTensor>& Storage::getValue(const QuantityId);
template Array<TracelessTensor>& Storage::getValue(const QuantityId);
template Array<Tensor>& Storage::getValue(const QuantityId);

template <typename TValue>
const Array<TValue>& Storage::getValue(const QuantityId key) const {
    return const_cast<Storage*>(this)->getValue<TValue>(key);
}

template const Array<Size>& Storage::getValue(const QuantityId) const;
template const Array<Float>& Storage::getValue(const QuantityId) const;
template const Array<Vector>& Storage::getValue(const QuantityId) const;
template const Array<SymmetricTensor>& Storage::getValue(const QuantityId) const;
template const Array<TracelessTensor>& Storage::getValue(const QuantityId) const;
template const Array<Tensor>& Storage::getValue(const QuantityId) const;

template <typename TValue>
Array<TValue>& Storage::getDt(const QuantityId key) {
    Quantity& q = this->getQuantity(key);
    checkStorageAccess(q.getValueEnum() == GetValueEnum<TValue>::type, key);
    return q.getDt<TValue>();
}

template Array<Size>& Storage::getDt(const QuantityId);
template Array<Float>& Storage::getDt(const QuantityId);
template Array<Vector>& Storage::getDt(const QuantityId);
template Array<SymmetricTensor>& Storage::getDt(const QuantityId);
template Array<TracelessTensor>& Storage::getDt(const QuantityId);
template Array<Tensor>& Storage::getDt(const QuantityId);

template <typename TValue>
const Array<TValue>& Storage::getDt(const QuantityId key) const {
    return const_cast<Storage*>(this)->getDt<TValue>(key);
}

template const Array<Size>& Storage::getDt(const QuantityId) const;
template const Array<Float>& Storage::getDt(const QuantityId) const;
template const Array<Vector>& Storage::getDt(const QuantityId) const;
template const Array<SymmetricTensor>& Storage::getDt(const QuantityId) const;
template const Array<TracelessTensor>& Storage::getDt(const QuantityId) const;
template const Array<Tensor>& Storage::getDt(const QuantityId) const;

template <typename TValue>
Array<TValue>& Storage::getD2t(const QuantityId key) {
    Quantity& q = this->getQuantity(key);
    checkStorageAccess(q.getValueEnum() == GetValueEnum<TValue>::type, key);
    return q.getD2t<TValue>();
}

template Array<Size>& Storage::getD2t(const QuantityId);
template Array<Float>& Storage::getD2t(const QuantityId);
template Array<Vector>& Storage::getD2t(const QuantityId);
template Array<SymmetricTensor>& Storage::getD2t(const QuantityId);
template Array<TracelessTensor>& Storage::getD2t(const QuantityId);
template Array<Tensor>& Storage::getD2t(const QuantityId);

template <typename TValue>
const Array<TValue>& Storage::getD2t(const QuantityId key) const {
    return const_cast<Storage*>(this)->getD2t<TValue>(key);
}

template const Array<Size>& Storage::getD2t(const QuantityId) const;
template const Array<Float>& Storage::getD2t(const QuantityId) const;
template const Array<Vector>& Storage::getD2t(const QuantityId) const;
template const Array<SymmetricTensor>& Storage::getD2t(const QuantityId) const;
template const Array<TracelessTensor>& Storage::getD2t(const QuantityId) const;
template const Array<Tensor>& Storage::getD2t(const QuantityId) const;


template <typename TValue>
Quantity& Storage::insert(const QuantityId key, const OrderEnum order, const TValue& defaultValue) {
    ASSERT(isReal(defaultValue));
    if (this->has(key)) {
        Quantity& q = this->getQuantity(key);
        checkStorageAccess(q.getValueEnum() == GetValueEnum<TValue>::type,
            "Inserting quantity already stored with different type");

        if (q.getOrderEnum() < order) {
            q.setOrder(order);
        }
    } else {
        const Size particleCnt = getParticleCnt();
        checkStorageAccess(particleCnt > 0, "Cannot insert quantity with default value to an empty storage.");
        quantities.insert(key, Quantity(order, defaultValue, particleCnt));
    }
    return quantities[key];
}

template Quantity& Storage::insert(const QuantityId, const OrderEnum, const Size&);
template Quantity& Storage::insert(const QuantityId, const OrderEnum, const Float&);
template Quantity& Storage::insert(const QuantityId, const OrderEnum, const Vector&);
template Quantity& Storage::insert(const QuantityId, const OrderEnum, const TracelessTensor&);
template Quantity& Storage::insert(const QuantityId, const OrderEnum, const SymmetricTensor&);
template Quantity& Storage::insert(const QuantityId, const OrderEnum, const Tensor&);

template <typename TValue>
Quantity& Storage::insert(const QuantityId key, const OrderEnum order, Array<TValue>&& values) {
    if (this->has(key)) {
        checkStorageAccess(values.size() == this->getParticleCnt(),
            "Size of input array must match number of particles in the storage.");

        Quantity& q = this->getQuantity(key);
        checkStorageAccess(q.getValueEnum() == GetValueEnum<TValue>::type,
            "Inserting quantity already stored with different type");

        if (q.getOrderEnum() < order) {
            q.setOrder(order);
        }
        q.getValue<TValue>() = std::move(values);
        if (key == QuantityId::MATERIAL_ID) {
            // matIds view has been invalidated, cache again
            this->update();
        }
    } else {
        Quantity q(order, std::move(values));
        const Size size = q.size();
        quantities.insert(key, std::move(q));
        checkStorageAccess(quantities.empty() || size == getParticleCnt(),
            "Size of input array must match number of particles in the storage.");

        if (this->getQuantityCnt() == 1 && this->getMaterialCnt() > 0) {
            // this is the first inserted quantity, initialize the 'internal' matId quantity
            Quantity& quantity = this->insert<Size>(QuantityId::MATERIAL_ID, OrderEnum::ZERO, 0);
            matIds = quantity.getValue<Size>();
            ASSERT(this->getMaterialCnt() == 1);
            mats[0].from = 0;
            mats[0].to = this->getParticleCnt();
        }
    }
    return quantities[key];
}

template Quantity& Storage::insert(const QuantityId, const OrderEnum, Array<Size>&&);
template Quantity& Storage::insert(const QuantityId, const OrderEnum, Array<Float>&&);
template Quantity& Storage::insert(const QuantityId, const OrderEnum, Array<Vector>&&);
template Quantity& Storage::insert(const QuantityId, const OrderEnum, Array<TracelessTensor>&&);
template Quantity& Storage::insert(const QuantityId, const OrderEnum, Array<SymmetricTensor>&&);
template Quantity& Storage::insert(const QuantityId, const OrderEnum, Array<Tensor>&&);

void Storage::addDependent(const WeakPtr<Storage>& other) {
#ifdef SPH_DEBUG
    // check for a cycle - look for itself in a hierarchy
    Function<bool(const Storage&)> checkDependent = [this, &checkDependent](const Storage& storage) {
        for (const WeakPtr<Storage>& weakPtr : storage.dependent) {
            if (SharedPtr<Storage> ptr = weakPtr.lock()) {
                if (&*ptr == this) {
                    return false;
                }
                const bool retval = checkDependent(*ptr);
                if (!retval) {
                    return false;
                }
            }
        }
        return true;
    };
    ASSERT(checkDependent(*this));
    if (SharedPtr<Storage> sharedPtr = other.lock()) {
        ASSERT(&*sharedPtr != this);
        ASSERT(checkDependent(*sharedPtr));
    }
#endif

    dependent.push(other);
}

MaterialView Storage::getMaterial(const Size matId) const {
    ASSERT(!mats.empty());
    const MatRange& mat = mats[matId];
    return MaterialView(mat.material.get(), IndexSequence(mat.from, mat.to));
}

MaterialView Storage::getMaterialOfParticle(const Size particleIdx) const {
    ASSERT(!mats.empty() && particleIdx < matIds.size());
    return this->getMaterial(matIds[particleIdx]);
}

void Storage::setMaterial(const Size matIdx, const SharedPtr<IMaterial>& material) {
    if (matIdx >= mats.size()) {
        throw InvalidStorageAccess("No material with index " + std::to_string(matIdx));
    }

    /// \todo merge material ranes if they have the same material
    mats[matIdx].material = material;
}

bool Storage::isHomogeneous(const BodySettingsId param) const {
    if (mats.empty()) {
        return true;
    }
    const MaterialView mat0 = this->getMaterial(0);
    const Float value0 = mat0->getParam<Float>(param);
    for (Size matId = 1; matId < this->getMaterialCnt(); ++matId) {
        const MaterialView mat = this->getMaterial(matId);
        const Float value = mat->getParam<Float>(param);
        if (value != value0) {
            return false;
        }
    }
    return true;
}

Interval Storage::getRange(const QuantityId id, const Size matIdx) const {
    ASSERT(matIdx < mats.size());
    return mats[matIdx].material->range(id);
}

template <typename TValue>
Array<TValue> Storage::getMaterialParams(const BodySettingsId param) const {
    Array<TValue> values;
    for (Size matId = 0; matId < this->getMaterialCnt(); ++matId) {
        values.push(this->getMaterial(matId)->getParam<TValue>(param));
    }
    return values;
}

StorageSequence Storage::getQuantities() {
    return *this;
}

ConstStorageSequence Storage::getQuantities() const {
    return *this;
}

void Storage::propagate(const Function<void(Storage& storage)>& functor) {
    for (Size i = 0; i < dependent.size();) {
        if (SharedPtr<Storage> storagePtr = dependent[i].lock()) {
            functor(*storagePtr);
            storagePtr->propagate(functor);
            ++i;
        } else {
            // remove expired storage
            dependent.remove(i);
        }
    }
}

Size Storage::getMaterialCnt() const {
    return mats.size();
}

Size Storage::getQuantityCnt() const {
    return quantities.size();
}

Size Storage::getParticleCnt() const {
    if (quantities.empty()) {
        return 0;
    } else {
        return quantities.begin()->value.size();
    }
}

bool Storage::empty() const {
    return this->getParticleCnt() == 0;
}

void Storage::addMissingBuffers(const Storage& source) {
    const Size cnt = this->getParticleCnt();
    for (ConstStorageElement element : source.getQuantities()) {
        // add the quantity if it's missing
        if (!this->has(element.id)) {
            quantities.insert(element.id, element.quantity.createZeros(cnt));
        }

        // if it has lower order, initialize the other buffers as well
        if (quantities[element.id].getOrderEnum() < element.quantity.getOrderEnum()) {
            quantities[element.id].setOrder(element.quantity.getOrderEnum());
        }
    }
}

void Storage::merge(Storage&& other) {
    ASSERT(!userData && !other.userData, "Merging storages with user data is currently not supported");

    // allow merging into empty storage for convenience
    if (this->getQuantityCnt() == 0) {
        *this = std::move(other);
        return;
    }

    // must have the same quantities
    this->addMissingBuffers(other);
    other.addMissingBuffers(*this);

    ASSERT(this->isValid() && other.isValid());


    // make sure that either both have materials or neither
    if (bool(this->getMaterialCnt()) != bool(other.getMaterialCnt())) {
        Storage* withoutMat = bool(this->getMaterialCnt()) ? &other : this;
        const BodySettings& body = BodySettings::getDefaults();
        withoutMat->mats.emplaceBack(makeShared<NullMaterial>(body), 0, other.getParticleCnt());
        withoutMat->insert<Size>(QuantityId::MATERIAL_ID, OrderEnum::ZERO, 0);
    }

    // update material intervals and cached matIds before merge
    const Size partCnt = this->getParticleCnt();
    for (MatRange& mat : other.mats) {
        mat.from += partCnt;
        mat.to += partCnt;
    }
    if (other.has(QuantityId::MATERIAL_ID)) {
        const Size matCnt = this->getMaterialCnt();
        for (Size& id : other.getValue<Size>(QuantityId::MATERIAL_ID)) {
            id += matCnt;
        }
    }

    // merge all quantities
    iteratePair<VisitorEnum::ALL_BUFFERS>(*this, other, [](auto& ar1, auto& ar2) { //
        ar1.pushAll(std::move(ar2));
    });

    // update persistent indices
    if (this->has(QuantityId::PERSISTENT_INDEX)) {
        ArrayView<Size> idxs = this->getValue<Size>(QuantityId::PERSISTENT_INDEX);
        const Size idx0 = idxs[partCnt - 1] + 1; // next available index
        for (Size i = partCnt; i < this->getParticleCnt(); ++i) {
            idxs[i] = idx0 + (i - partCnt);
        }
    }

    // merge materials
    this->mats.pushAll(std::move(other.mats));

    // remove duplicate materials (only consecutive, otherwise we would have to reorder particles)
    for (Size matId = 1; matId < this->getMaterialCnt();) {
        if (mats[matId].material == mats[matId - 1].material) {
            // same material, we can merge them
            mats[matId - 1].to = mats[matId].to;
            mats.remove(matId);

            if (this->has(QuantityId::MATERIAL_ID)) {
                ArrayView<Size> id = this->getValue<Size>(QuantityId::MATERIAL_ID);
                for (Size i = 0; i < id.size(); ++i) {
                    if (id[i] >= matId) {
                        id[i]--;
                    }
                }
            }
        } else {
            ++matId;
        }
    }

    // cache the view
    this->update();

    // since we moved the buffers away, remove all particles from other to keep it in consistent state
    other.removeAll();

    // sanity check
    ASSERT(this->isValid());
}

void Storage::zeroHighestDerivatives() {
    iterate<VisitorEnum::HIGHEST_DERIVATIVES>(*this, [](const QuantityId, auto& dv) {
        using TValue = typename std::decay_t<decltype(dv)>::Type;
        dv.fill(TValue(0._f));
    });
}

Storage Storage::clone(const Flags<VisitorEnum> buffers) const {
    ASSERT(!userData, "Cloning storages with user data is currently not supported");
    Storage cloned;
    for (const auto& q : quantities) {
        cloned.quantities.insert(q.key, q.value.clone(buffers));
    }

    // clone the materials if we cloned MATERIAL_IDs.
    if (cloned.has(QuantityId::MATERIAL_ID) && !cloned.getValue<Size>(QuantityId::MATERIAL_ID).empty()) {
        cloned.mats = this->mats.clone();
    }

    cloned.update();
    return cloned;
}

void Storage::resize(const Size newParticleCnt, const Flags<ResizeFlag> flags) {
    ASSERT(getQuantityCnt() > 0 && getMaterialCnt() <= 1);
    ASSERT(!userData, "Cloning storages with user data is currently not supported");

    iterate<VisitorEnum::ALL_BUFFERS>(*this, [newParticleCnt, flags](auto& buffer) { //
        if (!flags.has(ResizeFlag::KEEP_EMPTY_UNCHANGED) || !buffer.empty()) {
            using Type = typename std::decay_t<decltype(buffer)>::Type;
            buffer.resizeAndSet(newParticleCnt, Type(0._f));
        }
    });

    if (!mats.empty()) {
        // can be only used for homogeneous storages
        mats[0].to = newParticleCnt;
    }

    this->propagate([newParticleCnt, flags](Storage& storage) { storage.resize(newParticleCnt, flags); });

    this->update();
    ASSERT(this->isValid(
        flags.has(ResizeFlag::KEEP_EMPTY_UNCHANGED) ? Flags<ValidFlag>() : ValidFlag::COMPLETE));
}

void Storage::swap(Storage& other, const Flags<VisitorEnum> flags) {
    ASSERT(this->getQuantityCnt() == other.getQuantityCnt());
    for (auto i1 = quantities.begin(), i2 = other.quantities.begin(); i1 != quantities.end(); ++i1, ++i2) {
        i1->value.swap(i2->value, flags);
    }
}

Outcome Storage::isValid(const Flags<ValidFlag> flags) const {
    const Size cnt = this->getParticleCnt();
    Outcome result = SUCCESS;
    // check that all buffers have the same number of particles
    iterate<VisitorEnum::ALL_BUFFERS>(*this, [cnt, &result, flags](const auto& buffer) {
        if (buffer.size() != cnt && (flags.has(ValidFlag::COMPLETE) || !buffer.empty())) {
            result =
                "One or more buffers have different number of particles:\nExpected: " + std::to_string(cnt) +
                ", actual: " + std::to_string(buffer.size());
        }
    });
    if (!result) {
        return result;
    }

    // check that materials are set up correctly
    if (this->getMaterialCnt() == 0 || this->getQuantityCnt() == 0) {
        // no materials are a valid state, all OK
        return SUCCESS;
    }
    if (bool(matIds) != this->has(QuantityId::MATERIAL_ID)) {
        // checks that either both matIds and quantity MATERIAL_ID exist, or neither of them do;
        // if we cloned just some buffers, quantity MATERIAL_ID might not be there even though the storage has
        // materials, so we don't report it as an error.
        return "MaterialID view not present";
    }

    if (ArrayView<const Size>(matIds) != this->getValue<Size>(QuantityId::MATERIAL_ID)) {
        // WTF did I cache?
        return "Cached view of MaterialID does not reference the stored quantity. Did you forget to call "
               "update?";
    }

    for (Size matId = 0; matId < mats.size(); ++matId) {
        const MatRange& mat = mats[matId];
        for (Size i = mat.from; i < mat.to; ++i) {
            if (matIds[i] != matId) {
                return "MaterialID of particle does not belong to the material range.\nExpected: " +
                       std::to_string(matId) + ", actual: " + std::to_string(matIds[i]);
            }
        }
        if ((matId != mats.size() - 1) && (mat.to != mats[matId + 1].from)) {
            return "Material are not stored consecutively.\nLast index: " + std::to_string(mat.to) +
                   ", first index: " + std::to_string(mats[matId + 1].from);
        }
    }
    if (mats[0].from != 0 || mats[mats.size() - 1].to != cnt) {
        return "Materials do not cover all particles.\nFirst: " + std::to_string(mats[0].from) +
               ", last: " + std::to_string(mats[mats.size() - 1].to) + " (size: " + std::to_string(cnt) +
               ").";
    }

    return SUCCESS;
}

Array<Size> Storage::duplicate(ArrayView<const Size> idxs, const Flags<IndicesFlag> flags) {
    ASSERT(!userData, "Duplicating particles in storages with user data is currently not supported");
    MEASURE_SCOPE("Storage::duplicate");
    Array<Size> sortedHolder;
    ArrayView<const Size> sorted;
    if (flags.has(IndicesFlag::INDICES_SORTED)) {
        ASSERT(std::is_sorted(idxs.begin(), idxs.end()));
        sorted = idxs;
    } else {
        sortedHolder.reserve(idxs.size());
        sortedHolder.pushAll(idxs.begin(), idxs.end());
        std::sort(sortedHolder.begin(), sortedHolder.end());
        sorted = sortedHolder;
    }

    Array<Size> createdIdxs;
    if (this->has(QuantityId::MATERIAL_ID)) {

        // split by material
        Array<Array<Size>> idxsPerMaterial(this->getMaterialCnt());
        Array<Size>& matIdsRef = this->getValue<Size>(QuantityId::MATERIAL_ID);
        for (Size i : sorted) {
            idxsPerMaterial[matIdsRef[i]].push(i);
        }

        // add the new values after the last value of each material
        for (Array<Size>& idxs : reverse(idxsPerMaterial)) {
            if (idxs.empty()) {
                // no duplicates from this material
                continue;
            }
            const Size matId = matIdsRef[idxs[0]];
            iterate<VisitorEnum::ALL_BUFFERS>(*this, [this, &idxs, matId](auto& buffer) {
                using Type = typename std::decay_t<decltype(buffer)>::Type;
                Array<Type> duplicates;
                for (Size i : idxs) {
                    Type value = buffer[i];
                    duplicates.push(value);
                }
                buffer.insert(mats[matId].to, duplicates.begin(), duplicates.end());
            });

            for (Size& createdIdx : createdIdxs) {
                createdIdx += idxs.size();
            }
            for (Size i = 0; i < idxs.size(); ++i) {
                createdIdxs.push(mats[matId].to + i);
            }
        }

        // fix material ranges
        ASSERT(std::is_sorted(matIdsRef.begin(), matIdsRef.end()));
        for (Size matId = 0; matId < this->getMaterialCnt(); ++matId) {
            std::pair<Iterator<Size>, Iterator<Size>> range =
                std::equal_range(matIdsRef.begin(), matIdsRef.end(), matId);
            mats[matId].from = std::distance(matIdsRef.begin(), range.first);
            mats[matId].to = std::distance(matIdsRef.begin(), range.second);
        }
    } else {
        // just duplicate the particles
        const Size n0 = this->getParticleCnt();
        for (Size i = 0; i < sorted.size(); ++i) {
            createdIdxs.push(n0 + i);
        }
        iterate<VisitorEnum::ALL_BUFFERS>(*this, [&sorted](auto& buffer) {
            using Type = typename std::decay_t<decltype(buffer)>::Type;
            Array<Type> duplicates;
            for (Size i : sorted) {
                Type value = buffer[i];
                duplicates.push(value);
            }
            buffer.pushAll(duplicates.cbegin(), duplicates.cend());
        });
    }

    this->update();
    ASSERT(this->isValid(), this->isValid().error());

    std::sort(createdIdxs.begin(), createdIdxs.end());
    return createdIdxs;
}

void Storage::remove(ArrayView<const Size> idxs, const Flags<IndicesFlag> flags) {
    ASSERT(!userData, "Removing particles from storages with user data is currently not supported");
    if (idxs.empty()) {
        // job well done!
        return;
    }

    ArrayView<const Size> sortedIdxs;
    Array<Size> sortedHolder;
    if (flags.has(IndicesFlag::INDICES_SORTED)) {
        ASSERT(std::is_sorted(idxs.begin(), idxs.end()));
        sortedIdxs = idxs;
    } else {
        sortedHolder.pushAll(idxs.begin(), idxs.end());
        std::sort(sortedHolder.begin(), sortedHolder.end());
        sortedIdxs = sortedHolder;
    }

    iterate<VisitorEnum::ALL_BUFFERS>(*this, [&sortedIdxs](auto& buffer) { buffer.remove(sortedIdxs); });

    // update material ids
    this->update();

    // regenerate material ranges
    if (this->has(QuantityId::MATERIAL_ID)) {
        Array<Size> matsToRemove;
        for (Size matId = 0; matId < mats.size(); ++matId) {
            Iterator<Size> from, to;
            std::tie(from, to) = std::equal_range(matIds.begin(), matIds.end(), matId);
            if (from != matIds.end() && *from == matId) {
                // at least one particle from the material remained
                mats[matId].from = Size(std::distance(matIds.begin(), from));
                mats[matId].to = Size(std::distance(matIds.begin(), to));
            } else {
                // defer the removal to avoid changing indices
                matsToRemove.push(matId);
            }
        }
        mats.remove(matsToRemove);

    } else {
        ASSERT(mats.empty());
    }

    // in case some materials have been removed, we need to re-assign matIds
    for (Size matId = 0; matId < mats.size(); ++matId) {
        for (Size i = mats[matId].from; i < mats[matId].to; ++i) {
            matIds[i] = matId;
        }
    }

    ASSERT(this->isValid(), this->isValid().error());
}

void Storage::removeAll() {
    ASSERT(!userData, "Removing particles from storages with user data is currently not supported");
    this->propagate([](Storage& storage) { storage.removeAll(); });
    *this = Storage();
}

void Storage::update() {
    if (this->has(QuantityId::MATERIAL_ID)) {
        matIds = this->getValue<Size>(QuantityId::MATERIAL_ID);
    } else {
        matIds = nullptr;
    }
}

void Storage::setUserData(SharedPtr<IStorageUserData> newData) {
    userData = std::move(newData);
}

SharedPtr<IStorageUserData> Storage::getUserData() const {
    return userData;
}

Box getBoundingBox(const Storage& storage, const Float radius) {
    Box box;
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        box.extend(r[i] + radius * Vector(r[i][H]));
        box.extend(r[i] - radius * Vector(r[i][H]));
    }
    return box;
}

Vector getCenterOfMass(const Storage& storage) {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    if (storage.has(QuantityId::MASS)) {
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);

        Float m_sum = 0._f;
        Vector r_com(0._f);
        for (Size i = 0; i < r.size(); ++i) {
            m_sum += m[i];
            r_com += m[i] * r[i];
        }
        r_com[H] = 0._f;
        return r_com / m_sum;
    } else {
        Vector r_com(0._f);
        for (Size i = 0; i < r.size(); ++i) {
            r_com += r[i];
        }
        r_com[H] = 0._f;
        return r_com / r.size();
    }
}

void setPersistentIndices(Storage& storage) {
    const Size n = storage.getParticleCnt();
    Array<Size> idxs(n);
    for (Size i = 0; i < n; ++i) {
        idxs[i] = i;
    }
    storage.insert<Size>(QuantityId::PERSISTENT_INDEX, OrderEnum::ZERO, std::move(idxs));
}


NAMESPACE_SPH_END
