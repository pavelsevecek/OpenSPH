#include "sph/equations/Accumulated.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

template <typename TValue>
void Accumulated::insert(const QuantityId id, const OrderEnum order, const BufferSource source) {
    // add the buffer if not already present
    if (!this->hasBuffer(id, order)) {
        buffers.push(Element{ id, order, Array<TValue>() });
    }

    // check if we didn't call this more then once for 'unique' buffers
    auto recordIter = std::find_if(records.begin(), records.end(), [id](QuantityRecord& r) { //
        return r.id == id;
    });
    if (recordIter == records.end()) {
        records.push(QuantityRecord{ id, source == BufferSource::UNIQUE });
    } else {
        ASSERT(recordIter->id == id);
        if (source == BufferSource::UNIQUE || recordIter->unique) {
            // either the previous record was unique and we are adding another one, or the previous one
            // was shared and now we are adding unique
            ASSERT(false, "Another derivatives accumulates to quantity marked as unique");
        }
    }
}

template void Accumulated::insert<Size>(const QuantityId, const OrderEnum, const BufferSource);
template void Accumulated::insert<Float>(const QuantityId, const OrderEnum, const BufferSource);
template void Accumulated::insert<Vector>(const QuantityId, const OrderEnum, const BufferSource);
template void Accumulated::insert<SymmetricTensor>(const QuantityId, const OrderEnum, const BufferSource);
template void Accumulated::insert<TracelessTensor>(const QuantityId, const OrderEnum, const BufferSource);

void Accumulated::initialize(const Size size) {
    for (Element& e : buffers) {
        forValue(e.buffer, [size](auto& values) {
            using T = typename std::decay_t<decltype(values)>::Type;
            if (values.size() != size) {
                values.resize(size);
                values.fill(T(0._f));
            } else {
                // check that the array is really cleared
                ASSERT(std::count(values.begin(), values.end(), T(0._f)) == values.size());
            }
        });
    }
}

template <typename TValue>
Array<TValue>& Accumulated::getBuffer(const QuantityId id, const OrderEnum order) {
    for (Element& e : buffers) {
        if (e.id == id) {
            ASSERT(e.order == order);
            Array<TValue>& values = e.buffer;
            ASSERT(!values.empty());
            return values;
        }
    }
    STOP;
}

template Array<Size>& Accumulated::getBuffer(const QuantityId, const OrderEnum);
template Array<Float>& Accumulated::getBuffer(const QuantityId, const OrderEnum);
template Array<Vector>& Accumulated::getBuffer(const QuantityId, const OrderEnum);
template Array<SymmetricTensor>& Accumulated::getBuffer(const QuantityId, const OrderEnum);
template Array<TracelessTensor>& Accumulated::getBuffer(const QuantityId, const OrderEnum);

void Accumulated::sum(ArrayView<Accumulated*> others) {
    for (Element& e : buffers) {
        forValue(e.buffer, [this, &e, &others](auto& buffer) INL { //
            this->sumBuffer(buffer, e.id, others);
        });
    }
}

void Accumulated::sum(IScheduler& scheduler, ArrayView<Accumulated*> others) {
    for (Element& e : buffers) {
        forValue(e.buffer, [this, &scheduler, &e, &others](auto& buffer) INL { //
            this->sumBuffer(scheduler, buffer, e.id, others);
        });
    }
}
void Accumulated::store(Storage& storage) {
    for (Element& e : buffers) {
        forValue(e.buffer, [&e, &storage](auto& buffer) {
            using T = typename std::decay_t<decltype(buffer)>::Type;
            // storage must already have the quantity, we cannot add quantities during the run because of
            // timestepping
            ASSERT(storage.has(e.id), getMetadata(e.id).quantityName);
            ASSERT(Size(storage.getQuantity(e.id).getOrderEnum()) >= Size(e.order));
            storage.getAll<T>(e.id)[Size(e.order)] = std::move(buffer);
            buffer.fill(T(0._f));
        });
    }
}

Size Accumulated::getBufferCnt() const {
    return buffers.size();
}

template <typename Type>
Array<Iterator<Type>> Accumulated::getBufferIterators(const QuantityId id, ArrayView<Accumulated*> others) {
    Array<Iterator<Type>> iterators;
    for (Accumulated* other : others) {
        auto iter = std::find_if(other->buffers.begin(), other->buffers.end(), [id](const Element& e) { //
            return e.id == id;
        });
        ASSERT(iter != other->buffers.end());
        Array<Type>& buffer2 = iter->buffer;
        iterators.push(buffer2.begin());
    }
    return iterators;
}

template <typename Type>
void Accumulated::sumBuffer(Array<Type>& buffer1, const QuantityId id, ArrayView<Accumulated*> others) {
    Array<Iterator<Type>> iterators = this->getBufferIterators<Type>(id, others);
    const Type zero = Type(0._f);
    for (Size i = 0; i < buffer1.size(); ++i) {
        Type sum = zero;
        for (Iterator<Type>& iter : iterators) {
            if (*iter != zero) {
                sum += *iter;
                *iter = zero;
            }
            ++iter;
        }
        buffer1[i] += sum;
    }
}

template <typename Type>
void Accumulated::sumBuffer(IScheduler& scheduler,
    Array<Type>& buffer1,
    const QuantityId id,
    ArrayView<Accumulated*> others) {
    Array<Iterator<Type>> iterators = this->getBufferIterators<Type>(id, others);
    auto functor = [&iterators, &buffer1](const Size i) INL {
        Type sum(0._f);
        for (Iterator<Type> iter : iterators) {
            Type& x = *(iter + i);
            if (x != Type(0._f)) {
                sum += x;
                x = Type(0._f);
            }
        }
        buffer1[i] += sum;
    };
    parallelFor(scheduler, 0, buffer1.size(), functor);
}

bool Accumulated::hasBuffer(const QuantityId id, const OrderEnum order) const {
    for (const Element& e : buffers) {
        if (e.id == id) {
            // already used
            ASSERT(e.order == order, "Cannot accumulate both values and derivatives of quantity");
            return true;
        }
    }
    return false;
}

NAMESPACE_SPH_END
