#pragma once

#include "geometry/Tensor.h"
#include "objects/containers/LimitedArray.h"
#include "objects/wrappers/Flags.h"
#include "objects/wrappers/Optional.h"
#include "storage/QuantityHelpers.h"

NAMESPACE_SPH_BEGIN


namespace Detail {
    /// Abstract holder of all data associated with a quantity. Provides interface to extract information
    /// about the quantity. Must be static/dynamic_casted to one of derived types to extract information
    /// abount stored types or number of stored arrays.
    struct PlaceHolder : public Polymorphic {
        /// Returns number of derivatives stored within the quantity
        virtual TemporalEnum getTemporalEnum() const = 0;

        /// Return type of quantity values
        virtual ValueEnum getValueEnum() const = 0;

        /// Clones the quantity, optionally selecting arrays to clone; returns them as unique_ptr.
        virtual std::unique_ptr<PlaceHolder> clone(const Flags<TemporalEnum> flags) const = 0;

        /// Swaps arrays in two quantities, optionally selecting arrays to swap.
        virtual void swap(PlaceHolder* other, Flags<TemporalEnum> flags) = 0;
    };

    /// Abstract extension of PlaceHolder that adds interface to get all buffers of given type stored in the
    /// holder.
    template <typename TValue>
    struct ValueHolder : public PlaceHolder {
        virtual Array<LimitedArray<TValue>&> getBuffers() = 0;
    };

    template <typename TValue, TemporalEnum Type>
    class Holder;

    /// Holder of constant quantities or quantities values of which are NOT computed in timestepping using
    /// derivatives (pressure, sound speed, ...). This is also parent of holder with derivatives, it is
    /// therefore possible to cast first/second order holder to const holder and access its methods without
    /// error.
    template <typename TValue>
    class Holder<TValue, TemporalEnum::CONST> : public ValueHolder<TValue> {
    protected:
        LimitedArray<TValue> v;

        LimitedArray<TValue> conditionalClone(const LimitedArray<TValue>& array, const bool condition) const {
            if (condition) {
                return array.clone();
            } else {
                return LimitedArray<TValue>();
            }
        }

        void conditionalSwap(LimitedArray<TValue>& ar1,
                             LimitedArray<TValue>& ar2,
                             const bool condition) const {
            if (condition) {
                ar1.swap(ar2);
            }
        }

    public:
        Holder() = default;

        Holder(const LimitedArray<TValue>& v)
            : v(v.clone()) {}

        virtual TemporalEnum getTemporalEnum() const override { return TemporalEnum::CONST; }

        virtual ValueEnum getValueEnum() const override { return GetValueType<TValue>::type; }

        virtual std::unique_ptr<PlaceHolder> clone(const Flags<TemporalEnum> flags) const override {
            LimitedArray<TValue>&& cv =
                conditionalClone(v, flags.hasAny(TemporalEnum::CONST, TemporalEnum::ALL));
            return std::make_unique<Holder>(cv);
        }

        virtual void swap(PlaceHolder* other, Flags<TemporalEnum> flags) override {
            ASSERT((dynamic_cast<Holder<TValue, TemporalEnum::CONST>*>(other)));
            auto holder = static_cast<Holder<TValue, TemporalEnum::CONST>*>(other);
            conditionalSwap(v, holder->v, flags.hasAny(TemporalEnum::CONST, TemporalEnum::ALL));
        }

        virtual Array<LimitedArray<TValue>&> getBuffers() override { return { this->v }; }

        LimitedArray<TValue>& getValue() { return v; }
    };

    /// Holder for first-order quantities, contains also derivative of the quantity. The derivative can be
    /// accessed using getDerivative() method.
    template <typename TValue>
    class Holder<TValue, TemporalEnum::FIRST_ORDER> : public Holder<TValue, TemporalEnum::CONST> {
    protected:
        LimitedArray<TValue> dv;

    public:
        Holder() = default;

        Holder(const LimitedArray<TValue>& v)
            : Holder<TValue, TemporalEnum::CONST>(v) {
            dv.resize(this->v.size());
            dv.fill(TValue(0._f)); // fill derivative with zeroes
        }

        Holder(const LimitedArray<TValue>& v, const LimitedArray<TValue>& dv)
            : Holder<TValue, TemporalEnum::CONST>(v)
            , dv(dv.clone()) {}

        virtual TemporalEnum getTemporalEnum() const override { return TemporalEnum::FIRST_ORDER; }

        virtual ValueEnum getValueEnum() const override { return GetValueType<TValue>::type; }

        virtual std::unique_ptr<PlaceHolder> clone(const Flags<TemporalEnum> flags) const override {
            LimitedArray<TValue>&& cv =
                this->conditionalClone(this->v, flags.hasAny(TemporalEnum::CONST, TemporalEnum::ALL));
            LimitedArray<TValue>&& cdv = this->conditionalClone(this->dv,
                                                                flags.hasAny(TemporalEnum::FIRST_ORDER,
                                                                             TemporalEnum::HIGHEST_ORDER,
                                                                             TemporalEnum::ALL));
            return std::make_unique<Holder>(cv, cdv);
        }

        virtual void swap(PlaceHolder* other, Flags<TemporalEnum> flags) override {
            ASSERT((dynamic_cast<Holder<TValue, TemporalEnum::FIRST_ORDER>*>(other)));
            auto holder = static_cast<Holder<TValue, TemporalEnum::FIRST_ORDER>*>(other);
            this->conditionalSwap(this->v, holder->v, flags.hasAny(TemporalEnum::CONST, TemporalEnum::ALL));
            this->conditionalSwap(this->dv,
                                  holder->dv,
                                  flags.hasAny(TemporalEnum::FIRST_ORDER,
                                               TemporalEnum::HIGHEST_ORDER,
                                               TemporalEnum::ALL));
        }

        virtual Array<LimitedArray<TValue>&> getBuffers() override { return { this->v, this->dv }; }

        LimitedArray<TValue>& getDerivative() { return dv; }
    };

    /// Holder for second-order quantities, contains also second derivative of the quantity. The second
    /// derivative can be accessed using get2ndDerivative() method.
    template <typename TValue>
    class Holder<TValue, TemporalEnum::SECOND_ORDER> : public Holder<TValue, TemporalEnum::FIRST_ORDER> {
    private:
        LimitedArray<TValue> d2v;

    public:
        Holder() = default;

        Holder(const LimitedArray<TValue>& v)
            : Holder<TValue, TemporalEnum::FIRST_ORDER>(v) {
            d2v.resize(this->v.size());
            d2v.fill(TValue(0._f)); // fill derivative with zeroes
        }

        Holder(const LimitedArray<TValue>& v, const LimitedArray<TValue>& dv, const LimitedArray<TValue>& d2v)
            : Holder<TValue, TemporalEnum::FIRST_ORDER>(v, dv)
            , d2v(d2v.clone()) {}

        virtual TemporalEnum getTemporalEnum() const final { return TemporalEnum::SECOND_ORDER; }

        virtual ValueEnum getValueEnum() const final { return GetValueType<TValue>::type; }

        virtual std::unique_ptr<PlaceHolder> clone(const Flags<TemporalEnum> flags) const override {
            LimitedArray<TValue>&& cv =
                this->conditionalClone(this->v, flags.hasAny(TemporalEnum::CONST, TemporalEnum::ALL));
            LimitedArray<TValue>&& cdv =
                this->conditionalClone(this->dv, flags.hasAny(TemporalEnum::FIRST_ORDER, TemporalEnum::ALL));
            LimitedArray<TValue>&& cd2v = this->conditionalClone(d2v,
                                                                 flags.hasAny(TemporalEnum::SECOND_ORDER,
                                                                              TemporalEnum::HIGHEST_ORDER,
                                                                              TemporalEnum::ALL));
            return std::make_unique<Holder>(cv, cdv, cd2v);
        }

        virtual void swap(PlaceHolder* other, Flags<TemporalEnum> flags) override {
            ASSERT((dynamic_cast<Holder<TValue, TemporalEnum::SECOND_ORDER>*>(other)));
            auto holder = static_cast<Holder<TValue, TemporalEnum::SECOND_ORDER>*>(other);
            this->conditionalSwap(this->v, holder->v, flags.hasAny(TemporalEnum::CONST, TemporalEnum::ALL));
            this->conditionalSwap(this->dv,
                                  holder->dv,
                                  flags.hasAny(TemporalEnum::FIRST_ORDER, TemporalEnum::ALL));
            this->conditionalSwap(d2v,
                                  holder->d2v,
                                  flags.hasAny(TemporalEnum::SECOND_ORDER,
                                               TemporalEnum::HIGHEST_ORDER,
                                               TemporalEnum::ALL));
        }

        virtual Array<LimitedArray<TValue>&> getBuffers() override {
            return { this->v, this->dv, this->d2v };
        }

        LimitedArray<TValue>& get2ndDerivative() { return d2v; }
    };
}


/// Generic container for storing scalar, vector or tensor quantities. Containes current values of the unit
/// and all derivatives (if there is an evolution equation for the unit, of course).
/// As the quantity can have data of different types, there is no direct way to access the arrays stored
/// within. There are several methods, however, that allow to access the information indirectly:
/// 1) cast<Type, TemporalEnum>; returns holder of quantity (see above) IF the type and temporal enum in
///                              template parameters match the ones of the holder.
/// 2) getBuffers<Type>; returns all arrays (values and derivatives) stored in the holder IF the template type
///                      matches the holder type.
/// Beside accessing values through cast<> method, you can also use functions in QuantityCast namespace that
/// allow to access values or given derivative. The system is the same, though; we try to get
/// values/derivatives of given type and if they are stored within the quantity, they are returned.
class Quantity : public Noncopyable {
private:
    std::unique_ptr<Detail::PlaceHolder> data;
    int idx = -1;

public:
    Quantity() = default;

    Quantity(Quantity&& other) {
        std::swap(data, other.data);
        std::swap(idx, other.idx);
    }

    /// Creates a quantity from array of values and given type. If the type is 1st-order or 2nd-order,
    /// derivatives arrays resized to the same size as the array of values.
    template <typename TValue, TemporalEnum Type, typename... TArgs>
    void emplace(int key, TArgs&&... args) {
        using Holder = Detail::Holder<TValue, Type>;
        data         = std::make_unique<Holder>(std::forward<TArgs>(args)...);
        idx          = key;
    }


    Quantity& operator=(Quantity&& other) {
        std::swap(data, other.data);
        std::swap(idx, other.idx);
        return *this;
    }

    TemporalEnum getTemporalEnum() const {
        ASSERT(data);
        return data->getTemporalEnum();
    }

    ValueEnum getValueEnum() const {
        ASSERT(data);
        return data->getValueEnum();
    }

    int getKey() const { return idx; }

    Quantity clone(const Flags<TemporalEnum> flags) const {
        ASSERT(data);
        Quantity cloned;
        cloned.data = this->data->clone(flags);
        cloned.idx  = this->idx;
        return cloned;
    }

    void swap(Quantity& other, const Flags<TemporalEnum> flags) {
        ASSERT(data);
        this->data->swap(other.data.get(), flags);
        std::swap(this->idx, other.idx);
    }

    /// Casts quantity to given type and temporal enum and returns the holder if the quantity is indeed of
    /// given type and temporal enum, otherwise returns NOTHING. This CANNOT be used to check if the
    /// quantity
    /// is const or 1st order, as even 2nd order quantities can be successfully casted to const or 1st
    /// order.
    template <typename TValue, TemporalEnum Type>
    Optional<Detail::Holder<TValue, Type>&> cast() {
        using Holder   = Detail::Holder<TValue, Type>;
        Holder* holder = dynamic_cast<Holder*>(data.get());
        if (holder) {
            return *holder;
        } else {
            return NOTHING;
        }
    }

    /// Returns all buffers of given type stored in this quantity. If the quantity is of different type, an
    /// empty array is returned.
    template <typename TValue>
    Array<LimitedArray<TValue>&> getBuffers() {
        using Holder   = Detail::ValueHolder<TValue>;
        Holder* holder = dynamic_cast<Holder*>(data.get());
        if (holder) {
            return holder->getBuffers();
        } else {
            return {};
        }
    }
};

/*template <typename TValue, TemporalEnum Type>
Quantity makeQuantity() {
    Quantity q;
    q.emplace<TValue, Type>();
    return q;
}
*/
template <typename TMap>
Quantity makeQuantity() {
    Quantity q;
    q.emplace<typename TMap::Type, TMap::temporalEnum>();
    return q;
}

namespace QuantityCast {
    template <typename TValue>
    Optional<LimitedArray<TValue>&> get(Quantity& quantity) {
        auto holder = quantity.template cast<TValue, TemporalEnum::CONST>();
        if (!holder) {
            return NOTHING;
        } else {
            return holder->getValue();
        }
    }

    template <typename TValue>
    Optional<LimitedArray<TValue>&> dt(Quantity& quantity) {
        auto holder = quantity.template cast<TValue, TemporalEnum::FIRST_ORDER>();
        if (!holder) {
            return NOTHING;
        } else {
            return holder->getDerivative();
        }
    }

    template <typename TValue>
    Optional<LimitedArray<TValue>&> dt2(Quantity& quantity) {
        auto holder = quantity.template cast<TValue, TemporalEnum::SECOND_ORDER>();
        if (!holder) {
            return NOTHING;
        } else {
            return holder->get2ndDerivative();
        }
    }
}

NAMESPACE_SPH_END
