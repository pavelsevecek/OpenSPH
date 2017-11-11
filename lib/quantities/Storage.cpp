#include "quantities/Storage.h"
#include "physics/Eos.h"
#include "quantities/IMaterial.h"
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


Storage::Mat::Mat(AutoPtr<IMaterial>&& material, const Size from, const Size to)
    : material(std::move(material))
    , from(from)
    , to(to) {
    ASSERT(from < to || (from == 0 && to == 0));
}

Storage::Storage() = default;

Storage::Storage(AutoPtr<IMaterial>&& material) {
    mats.emplaceBack(std::move(material), 0, 0);
}

Storage::~Storage() = default;

Storage::Storage(Storage&& other) {
    *this = std::move(other);
}

Storage& Storage::operator=(Storage&& other) {
    quantities = std::move(other.quantities);
    mats = std::move(other.mats);
    dependent = std::move(other.dependent);

    if (this->getParticleCnt() > 0) {
        this->update();
    }
    return *this;
}

template <typename TValue>
Quantity& Storage::insert(const QuantityId key, const OrderEnum order, const TValue& defaultValue) {
    if (this->has(key)) {
        Quantity& q = this->getQuantity(key);
        if (q.getValueEnum() != GetValueEnum<TValue>::type) {
            throw InvalidSetup("Inserting quantity already stored with different type");
        }
        if (q.getOrderEnum() < order) {
            q.setOrder(order);
        }
    } else {
        const Size particleCnt = getParticleCnt();
        ASSERT(particleCnt);
        quantities[key] = Quantity(order, defaultValue, particleCnt);
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
        ASSERT(values.size() == this->getParticleCnt());
        Quantity& q = this->getQuantity(key);
        if (q.getValueEnum() != GetValueEnum<TValue>::type) {
            throw InvalidSetup("Inserting quantity already stored with different type");
        }
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
        UNUSED_IN_RELEASE(const Size size = q.size();)
        quantities[key] = std::move(q);
        ASSERT(quantities.empty() || size == getParticleCnt()); // size must match sizes of other quantities

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
    std::function<bool(const Storage&)> checkDependent = [this, &checkDependent](const Storage& storage) {
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
    const Mat& mat = mats[matId];
    return MaterialView(mat.material.get(), IndexSequence(mat.from, mat.to));
}

MaterialView Storage::getMaterialOfParticle(const Size particleIdx) const {
    ASSERT(!mats.empty() && particleIdx < matIds.size());
    return this->getMaterial(matIds[particleIdx]);
}

Interval Storage::getRange(const QuantityId id, const Size matIdx) const {
    ASSERT(matIdx < mats.size());
    return mats[matIdx].material->range(id);
}

StorageSequence Storage::getQuantities() {
    return *this;
}

ConstStorageSequence Storage::getQuantities() const {
    return *this;
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
        return quantities.begin()->second.size();
    }
}

void Storage::merge(Storage&& other) {
    // allow merging into empty storage for convenience
    if (this->getQuantityCnt() == 0) {
        *this = std::move(other);
        return;
    }

    // must have the same quantities
    ASSERT(this->getQuantityCnt() == other.getQuantityCnt());

    // either both have materials or neither
    ASSERT(bool(this->getMaterialCnt()) == bool(this->getMaterialCnt()));

    // update material intervals and cached matIds before merge
    const Size partCnt = this->getParticleCnt();
    for (Mat& mat : other.mats) {
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

    // merge materials
    this->mats.pushAll(std::move(other.mats));

    // cache the view
    this->update();

    // sanity check
    ASSERT(!matIds || matIds[0] == 0);
    ASSERT(!matIds || matIds[this->getParticleCnt() - 1] == this->getMaterialCnt() - 1);
    ASSERT(mats.empty() || mats[0].from == 0);
    ASSERT(mats.empty() || mats[mats.size() - 1].to == this->getParticleCnt());
}

void Storage::zeroHighestDerivatives() {
    iterate<VisitorEnum::HIGHEST_DERIVATIVES>(*this, [](const QuantityId, auto& dv) {
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

void Storage::resize(const Size newParticleCnt, const Flags<ResizeFlag> flags) {
    ASSERT(getQuantityCnt() > 0 && getMaterialCnt() <= 1);
    iterate<VisitorEnum::ALL_BUFFERS>(*this, [newParticleCnt, flags](auto& buffer) { //
        if (!flags.has(ResizeFlag::KEEP_EMPTY_UNCHANGED) || !buffer.empty()) {
            using Type = typename std::decay_t<decltype(buffer)>::Type;
            buffer.resizeAndSet(newParticleCnt, Type(0._f));
        }
    });

    for (WeakPtr<Storage>& weakPtr : dependent) {
        if (SharedPtr<Storage> ptr = weakPtr.lock()) {
            ptr->resize(newParticleCnt, flags);
        }
    }

    this->update();
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
    iterate<VisitorEnum::ALL_BUFFERS>(const_cast<Storage&>(*this), [cnt, &valid](const auto& buffer) {
        if (buffer.size() != cnt) {
            valid = false;
        }
    });
    return valid;
}

void Storage::remove(ArrayView<const Size> idxs) {
    Size particlesRemoved = 0;
    for (Size matId = 0; matId < mats.size();) {
        Mat& mat = mats[matId];
        mat.from -= particlesRemoved;
        for (Size i : idxs) {
            if (matIds[i] == matId) {
                mat.to--;
                particlesRemoved++;
            }
        }

        if (mat.from == mat.to) {
            // no particles with this material left, remove
            mats.remove(matId);
        } else {
            ++matId;
        }
    }

    iterate<VisitorEnum::ALL_BUFFERS>(*this, [idxs](auto& buffer) {
        for (Size i : idxs) {
            buffer.remove(i);
        }
    });

    this->update();

    NOT_IMPLEMENTED; /// \todo TESTS!!
}

void Storage::removeAll() {
    for (WeakPtr<Storage>& weakPtr : dependent) {
        if (SharedPtr<Storage> ptr = weakPtr.lock()) {
            ptr->removeAll();
        }
    }

    *this = Storage();
}

void Storage::update() {
    if (this->getMaterialCnt() > 0) {
        matIds = this->getValue<Size>(QuantityId::MATERIAL_ID);
    }
}

NAMESPACE_SPH_END
