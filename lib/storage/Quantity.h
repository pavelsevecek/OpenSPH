#pragma once

#include "objects/containers/Array.h"
#include "objects/wrappers/Flags.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

/// \todo maybe split into levels and number of derivatives?
enum class TemporalEnum {
    CONST        = 1 << 0, ///< Quantity without derivatives, or "zero order" of quantity
    FIRST_ORDER  = 1 << 1, ///< Quantity with 1st derivative
    SECOND_ORDER = 1 << 2, ///< Quantity with 1st and 2nd derivative
    /// additional helper flags for getters
    ALL           = 1 << 3, ///< All values and derivatives of all quantities
    HIGHEST_ORDER = 1 << 4, ///< Derivative with the highest order; nothing for const quantities
};

enum class ValueEnum { SCALAR, VECTOR, TENSOR, TRACELESS_TENSOR };


/// Convert type to ValueType enum
template <typename T>
struct GetValueType;
template <>
struct GetValueType<Float> {
    static constexpr ValueEnum type = ValueEnum::SCALAR;
};
template <>
struct GetValueType<Vector> {
    static constexpr ValueEnum type = ValueEnum::VECTOR;
};

/// Convert ValueType enum to type
template <ValueEnum Type>
struct GetTypeFromValue;
template <>
struct GetTypeFromValue<ValueEnum::SCALAR> {
    using Type = Float;
};
template <>
struct GetTypeFromValue<ValueEnum::VECTOR> {
    using Type = Vector;
};


namespace Detail {
    struct PlaceHolder : public Polymorphic {
        virtual TemporalEnum getTemporalType() const = 0;
        virtual ValueEnum getValueType() const       = 0;
        virtual std::unique_ptr<PlaceHolder> clone(const Flags<TemporalEnum> flags = EMPTY_FLAGS) const = 0;
    };

    template <typename TValue>
    struct ValueHolder : public PlaceHolder {
        virtual Array<Array<TValue>&> getBuffers() = 0;
    };

    template <typename TValue, TemporalEnum Type>
    class Holder;

    template <typename TValue>
    class Holder<TValue, TemporalEnum::CONST> : public ValueHolder<TValue> {
    protected:
        Array<TValue> v;

    public:
        Holder() = default;

        Holder(const Array<TValue>& v)
            : v(v.clone()) {}

        virtual TemporalEnum getTemporalType() const override { return TemporalEnum::CONST; }

        virtual ValueEnum getValueType() const override { return GetValueType<TValue>::type; }

        virtual std::unique_ptr<PlaceHolder> clone(
            const Flags<TemporalEnum> flags = EMPTY_FLAGS) const override {
            // if (flags.has)
            return std::make_unique<Holder>(v.clone());
        }

        virtual Array<Array<TValue>&> getBuffers() override { return { this->v }; }

        Array<TValue>& getValue() { return v; }
    };


    template <typename TValue>
    class Holder<TValue, TemporalEnum::FIRST_ORDER> : public Holder<TValue, TemporalEnum::CONST> {
    protected:
        Array<TValue> dv;

    public:
        Holder() = default;

        Holder(const Array<TValue>& v)
            : Holder<TValue, TemporalEnum::CONST>(v) {
            dv.resize(this->v.size());
        }

        Holder(const Array<TValue>& v, const Array<TValue>& dv)
            : Holder<TValue, TemporalEnum::CONST>(v)
            , dv(dv.clone()) {}

        virtual TemporalEnum getTemporalType() const override { return TemporalEnum::FIRST_ORDER; }

        virtual ValueEnum getValueType() const override { return GetValueType<TValue>::type; }

        virtual std::unique_ptr<PlaceHolder> clone(
            const Flags<TemporalEnum> flags = EMPTY_FLAGS) const override {
            return std::make_unique<Holder>(this->v.clone(), dv.clone());
        }

        virtual Array<Array<TValue>&> getBuffers() override { return { this->v, this->dv }; }

        Array<TValue>& getDerivative() { return dv; }
    };

    template <typename TValue>
    class Holder<TValue, TemporalEnum::SECOND_ORDER> : public Holder<TValue, TemporalEnum::FIRST_ORDER> {
    private:
        Array<TValue> d2v;

    public:
        Holder() = default;

        Holder(const Array<TValue>& v)
            : Holder<TValue, TemporalEnum::FIRST_ORDER>(v) {
            d2v.resize(this->v.size());
        }

        Holder(const Array<TValue>& v, const Array<TValue>& dv, const Array<TValue>& d2v)
            : Holder<TValue, TemporalEnum::FIRST_ORDER>(v, dv)
            , d2v(d2v.clone()) {}

        virtual TemporalEnum getTemporalType() const final { return TemporalEnum::SECOND_ORDER; }

        virtual ValueEnum getValueType() const final { return GetValueType<TValue>::type; }

        virtual std::unique_ptr<PlaceHolder> clone(
            const Flags<TemporalEnum> flags = EMPTY_FLAGS) const final {
            return std::make_unique<Holder>(this->v.clone(), this->dv.clone(), d2v.clone());
        }

        virtual Array<Array<TValue>&> getBuffers() override { return { this->v, this->dv, this->d2v }; }

        Array<TValue>& get2ndDerivative() { return d2v; }
    };
}


/// Generic container for storing scalar, vector or tensor arrays and its temporal derivatives.
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
        return data->getTemporalType();
    }

    ValueEnum getValueType() const {
        ASSERT(data);
        return data->getValueType();
    }

    int getKey() const { return idx; }

    Quantity clone() const {
        ASSERT(data);
        Quantity cloned;
        cloned.data = this->data->clone();
        cloned.idx = this->idx;
        return cloned;
    }

    /// Casts quantity to given type and temporal enum and returns the holder if the quantity is indeed of
    /// given type and temporal enum, otherwise returns NOTHING. This CANNOT be used to check if the quantity
    /// is const or 1st order, as even 2nd order quantities can be successfully casted to const or 1st order.
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

    template <typename TValue>
    Array<Array<TValue>&> getBuffers() {
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
    Optional<Array<TValue>&> get(Quantity& quantity) {
        auto holder = quantity.template cast<TValue, TemporalEnum::CONST>();
        if (!holder) {
            return NOTHING;
        } else {
            return holder->getValue();
        }
    }

    template <typename TValue>
    Optional<Array<TValue>&> dt(Quantity& quantity) {
        auto holder = quantity.template cast<TValue, TemporalEnum::FIRST_ORDER>();
        if (!holder) {
            return NOTHING;
        } else {
            return holder->getDerivative();
        }
    }

    template <typename TValue>
    Optional<Array<TValue>&> dt2(Quantity& quantity) {
        auto holder = quantity.template cast<TValue, TemporalEnum::SECOND_ORDER>();
        if (!holder) {
            return NOTHING;
        } else {
            return holder->get2ndDerivative();
        }
    }
}

NAMESPACE_SPH_END
