#include "quantities/Quantity.h"

NAMESPACE_SPH_BEGIN

namespace Detail {

template <typename TValue>
Holder<TValue> Holder<TValue>::clone(const Flags<VisitorEnum> flags) const {
    Holder cloned(order);
    visitConst(cloned, flags, [](const Array<TValue>& array, Array<TValue>& clonedArray) {
        clonedArray = array.clone();
    });
    return cloned;
}

template <typename TValue>
Holder<TValue> Holder<TValue>::createZeros(const Size particleCnt) const {
    return Holder(order, TValue(0._f), particleCnt);
}

template <typename TValue>
void Holder<TValue>::swap(Holder& other, Flags<VisitorEnum> flags) {
    visitMutable(other, flags, [](Array<TValue>& ar1, Array<TValue>& ar2) { ar1.swap(ar2); });
}

template <typename TValue>
void Holder<TValue>::setOrder(const OrderEnum newOrder) {
    ASSERT(Size(newOrder) > Size(order));
    if (newOrder == OrderEnum::SECOND) {
        d2v_dt2.resize(v.size());
        d2v_dt2.fill(TValue(0._f));
    } else if (newOrder == OrderEnum::FIRST) {
        dv_dt.resize(v.size());
        dv_dt.fill(TValue(0._f));
    }
    order = newOrder;
}

template <typename TValue>
void Holder<TValue>::initDerivatives(const Size size) {
    switch (order) {
    case OrderEnum::SECOND:
        d2v_dt2.resize(size);
        d2v_dt2.fill(TValue(0._f));
        dv_dt.resize(size);
        dv_dt.fill(TValue(0._f));
        // [[fallthrough]] - somehow doesn't work with gcc6
        break;
    case OrderEnum::FIRST:
        dv_dt.resize(size);
        dv_dt.fill(TValue(0._f));
        break;
    default:
        break;
    }
}

template <typename TValue>
template <typename TFunctor>
void Holder<TValue>::visitMutable(Holder& other, const Flags<VisitorEnum> flags, TFunctor&& functor) {
    if (flags.hasAny(VisitorEnum::ZERO_ORDER,
            VisitorEnum::ALL_BUFFERS,
            VisitorEnum::ALL_VALUES,
            VisitorEnum::STATE_VALUES)) {
        functor(v, other.v);
    }
    switch (order) {
    case OrderEnum::FIRST:
        if (flags.hasAny(
                VisitorEnum::FIRST_ORDER, VisitorEnum::ALL_BUFFERS, VisitorEnum::HIGHEST_DERIVATIVES)) {
            functor(dv_dt, other.dv_dt);
        }
        break;
    case OrderEnum::SECOND:
        if (flags.hasAny(VisitorEnum::ALL_BUFFERS, VisitorEnum::STATE_VALUES)) {
            functor(dv_dt, other.dv_dt);
        }
        if (flags.hasAny(
                VisitorEnum::ALL_BUFFERS, VisitorEnum::SECOND_ORDER, VisitorEnum::HIGHEST_DERIVATIVES)) {
            functor(d2v_dt2, other.d2v_dt2);
        }
        break;
    default:
        break;
    }
}

template <typename TValue>
template <typename TFunctor>
void Holder<TValue>::visitConst(Holder& other, const Flags<VisitorEnum> flags, TFunctor&& functor) const {
    const_cast<Holder*>(this)->visitMutable(other, flags, std::forward<TFunctor>(functor));
}

template class Holder<Size>;
template class Holder<Float>;
template class Holder<Vector>;
template class Holder<SymmetricTensor>;
template class Holder<TracelessTensor>;
template class Holder<Tensor>;

} // namespace Detail


NAMESPACE_SPH_END
