#pragma once

#include "objects/containers/Array.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

enum class TemporalType { CONST, FIRST_ORDER, SECOND_ORDER };

enum class ValueType { SCALAR, VECTOR, TENSOR, TRACELESS_TENSOR };

template <typename T>
struct GetValueType;

template <>
struct GetValueType<Float> {
    static constexpr ValueType type = ValueType::SCALAR;
};
template <>
struct GetValueType<Vector> {
    static constexpr ValueType type = ValueType::VECTOR;
};


namespace Detail {
    struct PlaceHolder : public Polymorphic {
        virtual TemporalType getTemporalType() const       = 0;
        virtual ValueType getValueType() const             = 0;
        virtual std::unique_ptr<PlaceHolder> clone() const = 0;
    };

    template <typename TValue>
    struct ValueHolder : public PlaceHolder {
        virtual Array<Array<TValue>&> getBuffers() = 0;
    };

    template <typename TValue, TemporalType Type>
    class Holder;

    template <typename TValue>
    class Holder<TValue, TemporalType::CONST> : public ValueHolder<TValue> {
    protected:
        Array<TValue> v;

    public:
        Holder() = default;

        Holder(const Array<TValue>& v)
            : v(v.clone()) {}

        virtual TemporalType getTemporalType() const override { return TemporalType::CONST; }

        virtual ValueType getValueType() const override { return GetValueType<TValue>::type; }

        virtual std::unique_ptr<PlaceHolder> clone() const override {
            return std::make_unique<Holder>(v.clone());
        }

        virtual Array<Array<TValue>&> getBuffers() override {
            return { this->v };
        }

        Array<TValue>& getValue() { return v; }
    };


    template <typename TValue>
    class Holder<TValue, TemporalType::FIRST_ORDER> : public Holder<TValue, TemporalType::CONST> {
    protected:
        Array<TValue> dv;

    public:
        Holder() = default;

        Holder(const Array<TValue>& v)
            : Holder<TValue, TemporalType::CONST>(v) {
            dv.resize(this->v.size());
        }

        Holder(const Array<TValue>& v, const Array<TValue>& dv)
            : Holder<TValue, TemporalType::CONST>(v)
            , dv(dv.clone()) {}

        virtual TemporalType getTemporalType() const override { return TemporalType::FIRST_ORDER; }

        virtual ValueType getValueType() const override { return GetValueType<TValue>::type; }

        virtual std::unique_ptr<PlaceHolder> clone() const override {
            return std::make_unique<Holder>(this->v.clone(), dv.clone());
        }

        virtual Array<Array<TValue>&> getBuffers() override {
            return { this->v, this->dv };
        }

        Array<TValue>& getDerivative() { return dv; }
    };

    template <typename TValue>
    class Holder<TValue, TemporalType::SECOND_ORDER> : public Holder<TValue, TemporalType::FIRST_ORDER> {
    private:
        Array<TValue> d2v;

    public:
        Holder() = default;

        Holder(const Array<TValue>& v)
            : Holder<TValue, TemporalType::FIRST_ORDER>(v) {
            d2v.resize(this->v.size());
        }

        Holder(const Array<TValue>& v, const Array<TValue>& dv, const Array<TValue>& d2v)
            : Holder<TValue, TemporalType::FIRST_ORDER>(v, dv)
            , d2v(d2v.clone()) {}

        virtual TemporalType getTemporalType() const final { return TemporalType::SECOND_ORDER; }

        virtual ValueType getValueType() const final { return GetValueType<TValue>::type; }

        virtual std::unique_ptr<PlaceHolder> clone() const final {
            return std::make_unique<Holder>(this->v.clone(), this->dv.clone(), d2v.clone());
        }

        virtual Array<Array<TValue>&> getBuffers() override {
            return { this->v, this->dv, this->d2v };
        }

        Array<TValue>& get2ndDerivative() { return d2v; }
    };
}


/// Generic container for storing scalar, vector or tensor arrays and its temporal derivatives.
class Quantity : public Noncopyable {
private:
    std::unique_ptr<Detail::PlaceHolder> data;

public:
    Quantity() = default;

    Quantity(Quantity&& other) { std::swap(data, other.data); }

    /// Creates a quantity from array of values and given type. If the type is 1st-order or 2nd-order,
    /// derivatives arrays resized to the same size as the array of values.
    template <TemporalType Type, typename TValue>
    Quantity(const Array<TValue>& array) {
        using Holder = Detail::Holder<TValue, Type>;
        data         = std::make_unique<Holder>(array);
    }

    Quantity& operator=(Quantity&& other) {
        std::swap(data, other.data);
        return *this;
    }

    TemporalType getTemporalType() const {
        ASSERT(data);
        return data->getTemporalType();
    }

    ValueType getValueType() const {
        ASSERT(data);
        return data->getValueType();
    }

    /// Creates a quantity given value type and number of temporal derivatives
    template <typename TValue, TemporalType Type>
    void emplace() {
        data = std::make_unique<Detail::Holder<TValue, Type>>();
    }

    Quantity clone() const {
        ASSERT(data);
        Quantity cloned;
        cloned.data = this->data->clone();
        return cloned;
    }

    template <typename TValue, TemporalType Type>
    Optional<Detail::Holder<TValue, Type>&> cast() {
        using Holder   = Detail::Holder<TValue, Type>;
        Holder* holder = dynamic_cast<Holder*>(data.get());
        if (holder) {
            return *holder;
        } else {
            return NOTHING;
        }
    }

    template<typename TValue>
    Array<Array<TValue>&> getBuffers() {
        using Holder = Detail::ValueHolder<TValue>;
        Holder* holder = dynamic_cast<Holder*>(data.get());
        if (holder) {
            return holder->getBuffers();
        } else {
            return {};
        }
    }
};

template <typename TValue, TemporalType Type>
Quantity makeQuantity() {
    Quantity q;
    q.emplace<TValue, Type>();
    return q;
}

namespace QuantityCast {
    template <typename TValue>
    Optional<Array<TValue>&> get(Quantity& quantity) {
        auto holder = quantity.template cast<TValue, TemporalType::CONST>();
        if (!holder) {
            return NOTHING;
        } else {
            return holder->getValue();
        }
    }

    template <typename TValue>
    Optional<Array<TValue>&> dt(Quantity& quantity) {
        auto holder = quantity.template cast<TValue, TemporalType::FIRST_ORDER>();
        if (!holder) {
            return NOTHING;
        } else {
            return holder->getDerivative();
        }
    }

    template <typename TValue>
    Optional<Array<TValue>&> dt2(Quantity& quantity) {
        auto holder = quantity.template cast<TValue, TemporalType::SECOND_ORDER>();
        if (!holder) {
            return NOTHING;
        } else {
            return holder->get2ndDerivative();
        }
    }
}

/*
template<TemporalType Type, typename TFunctor>
void iterate(Array<Quantity>& storage, TFunctor&& functor) {
    for (Quantity& q : storaage) {
        switch (q.getValueType()) {
        case ValueType::VECTOR:
            functor()
    }
}*/


NAMESPACE_SPH_END
