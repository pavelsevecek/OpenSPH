#pragma once

#include "geometry/Tensor.h"
#include "objects/containers/LimitedArray.h"
#include "objects/wrappers/Flags.h"
#include "objects/wrappers/Optional.h"
#include "storage/QuantityHelpers.h"
#include "storage/QuantityKey.h"

NAMESPACE_SPH_BEGIN


/// \todo maybe split into levels and number of derivatives?
enum class OrderEnum {
    ZERO_ORDER,   ///< Quantity without derivatives, or "zero order" of quantity
    FIRST_ORDER,  ///< Quantity with 1st derivative
    SECOND_ORDER, ///< Quantity with 1st and 2nd derivative
};


/// Types of iteration over storage
enum class VisitorEnum {
    /// Iterates only over const quantities or quantities with no derivatives. Passes the values as argument
    /// of functor.
    /// \note To iterate over all quantities and pass their values into the functor, use ALL_VALUES
    ZERO_ORDER = 1 << 0,

    /// Iterates only over first-order quantities. Passes values and derivatives as arguments of functor.
    FIRST_ORDER = 1 << 1,

    /// Iterates only over second-order quantities. Passes values, 1st derivatives and 2nd derivatives as
    /// arguments of functor.
    SECOND_ORDER = 1 << 2,

    /// Iterates over all stored arrays of all quantities. Executes functor for each value array and each
    /// derivative array.
    ALL_BUFFERS = 1 << 3,

    /// Iterates over all quantities, but executes the functor for values only (derivatives are not passed for
    /// higher-order quantities).
    ALL_VALUES = 1 << 4,

    /// Iterates over all 1st order and 2nd order quantities, passes their 1st and 2nd derivatives as
    /// parameters, respectively.
    HIGHEST_DERIVATIVES = 1 << 5,
};


namespace Detail {
    /// Abstract holder of all data associated with a quantity. Provides interface to extract information
    /// about the quantity. Must be static/dynamic_casted to one of derived types to extract information
    /// abount stored types or number of stored arrays.
    struct PlaceHolder : public Polymorphic {
        /// Returns number of derivatives stored within the quantity
        virtual OrderEnum getOrderEnum() const = 0;

        /// Return type of quantity values
        virtual ValueEnum getValueEnum() const = 0;

        /// Clones the quantity, optionally selecting arrays to clone; returns them as unique_ptr.
        virtual std::unique_ptr<PlaceHolder> clone(const Flags<VisitorEnum> flags) const = 0;

        /// Swaps arrays in two quantities, optionally selecting arrays to swap.
        virtual void swap(PlaceHolder* other, Flags<VisitorEnum> flags) = 0;
    };

    /// Abstract extension of PlaceHolder that adds interface to get all buffers of given type stored in the
    /// holder.
    template <typename TValue>
    struct ValueHolder : public PlaceHolder {
        virtual Array<LimitedArray<TValue>&> getBuffers() = 0;
    };

    template <typename TValue, OrderEnum Type>
    class Holder;

    /// Holder of constant quantities or quantities values of which are NOT computed in timestepping using
    /// derivatives (pressure, sound speed, ...). This is also parent of holder with derivatives, it is
    /// therefore possible to cast first/second order holder to const holder and access its methods without
    /// error.
    template <typename TValue>
    class Holder<TValue, OrderEnum::ZERO_ORDER> : public ValueHolder<TValue> {
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

        Holder(const int size, const TValue& value) {
            v.resize(size);
            v.fill(value);
        }

        template <typename TDistribution>
        Holder(TDistribution&& distribution) {
            v = std::forward<TDistribution>(distribution);
        }

        virtual OrderEnum getOrderEnum() const override { return OrderEnum::ZERO_ORDER; }

        virtual ValueEnum getValueEnum() const override { return GetValueType<TValue>::type; }

        virtual std::unique_ptr<PlaceHolder> clone(const Flags<VisitorEnum> flags) const override {
            LimitedArray<TValue>&& cv =
                conditionalClone(v, flags.hasAny(VisitorEnum::ZERO_ORDER, VisitorEnum::ALL_BUFFERS));
            return std::make_unique<Holder>(cv);
        }

        virtual void swap(PlaceHolder* other, Flags<VisitorEnum> flags) override {
            ASSERT((dynamic_cast<Holder<TValue, OrderEnum::ZERO_ORDER>*>(other)));
            auto holder = static_cast<Holder<TValue, OrderEnum::ZERO_ORDER>*>(other);
            conditionalSwap(v, holder->v, flags.hasAny(VisitorEnum::ZERO_ORDER, VisitorEnum::ALL_BUFFERS));
        }

        virtual Array<LimitedArray<TValue>&> getBuffers() override { return { this->v }; }

        LimitedArray<TValue>& getValue() { return v; }
    };

    /// Holder for first-order quantities, contains also derivative of the quantity. The derivative can be
    /// accessed using getDerivative() method.
    template <typename TValue>
    class Holder<TValue, OrderEnum::FIRST_ORDER> : public Holder<TValue, OrderEnum::ZERO_ORDER> {
    protected:
        LimitedArray<TValue> dv;

    public:
        Holder() = default;

        Holder(const int size, const TValue& value)
            : Holder<TValue, OrderEnum::ZERO_ORDER>(size, value) {
            dv.resize(this->v.size());
            dv.fill(TValue(0._f));
        }

        template <typename TDistribution>
        Holder(TDistribution&& distribution)
            : Holder<TValue, OrderEnum::ZERO_ORDER>(std::forward<TDistribution>(distribution)) {
            dv.resize(this->v.size());
            dv.fill(TValue(0._f));
        }

        virtual OrderEnum getOrderEnum() const override { return OrderEnum::FIRST_ORDER; }

        virtual ValueEnum getValueEnum() const override { return GetValueType<TValue>::type; }

        virtual std::unique_ptr<PlaceHolder> clone(const Flags<VisitorEnum> flags) const override {
            LimitedArray<TValue>&& cv =
                this->conditionalClone(this->v,
                                       flags.hasAny(VisitorEnum::ZERO_ORDER, VisitorEnum::ALL_BUFFERS));
            LimitedArray<TValue>&& cdv = this->conditionalClone(this->dv,
                                                                flags.hasAny(VisitorEnum::FIRST_ORDER,
                                                                             VisitorEnum::HIGHEST_DERIVATIVES,
                                                                             VisitorEnum::ALL_BUFFERS));
            return std::make_unique<Holder>(cv, cdv);
        }

        virtual void swap(PlaceHolder* other, Flags<VisitorEnum> flags) override {
            ASSERT((dynamic_cast<Holder<TValue, OrderEnum::FIRST_ORDER>*>(other)));
            auto holder = static_cast<Holder<TValue, OrderEnum::FIRST_ORDER>*>(other);
            this->conditionalSwap(this->v,
                                  holder->v,
                                  flags.hasAny(VisitorEnum::ZERO_ORDER, VisitorEnum::ALL_BUFFERS));
            this->conditionalSwap(this->dv,
                                  holder->dv,
                                  flags.hasAny(VisitorEnum::FIRST_ORDER,
                                               VisitorEnum::HIGHEST_DERIVATIVES,
                                               VisitorEnum::ALL_BUFFERS));
        }

        virtual Array<LimitedArray<TValue>&> getBuffers() override { return { this->v, this->dv }; }

        LimitedArray<TValue>& getDerivative() { return dv; }
    };

    /// Holder for second-order quantities, contains also second derivative of the quantity. The second
    /// derivative can be accessed using get2ndDerivative() method.
    template <typename TValue>
    class Holder<TValue, OrderEnum::SECOND_ORDER> : public Holder<TValue, OrderEnum::FIRST_ORDER> {
    private:
        LimitedArray<TValue> d2v;

    public:
        Holder() = default;

        Holder(const int size, const TValue& value)
            : Holder<TValue, OrderEnum::FIRST_ORDER>(size, value) {
            d2v.resize(this->v.size());
            d2v.fill(TValue(0._f));
        }

        template <typename TDistribution>
        Holder(TDistribution&& distribution)
            : Holder<TValue, OrderEnum::FIRST_ORDER>(std::forward<TDistribution>(distribution)) {
            d2v.resize(this->v.size());
            d2v.fill(TValue(0._f));
        }

        virtual OrderEnum getOrderEnum() const final { return OrderEnum::SECOND_ORDER; }

        virtual ValueEnum getValueEnum() const final { return GetValueType<TValue>::type; }

        virtual std::unique_ptr<PlaceHolder> clone(const Flags<VisitorEnum> flags) const override {
            LimitedArray<TValue>&& cv =
                this->conditionalClone(this->v,
                                       flags.hasAny(VisitorEnum::ZERO_ORDER, VisitorEnum::ALL_BUFFERS));
            LimitedArray<TValue>&& cdv =
                this->conditionalClone(this->dv,
                                       flags.hasAny(VisitorEnum::FIRST_ORDER, VisitorEnum::ALL_BUFFERS));
            LimitedArray<TValue>&& cd2v =
                this->conditionalClone(d2v,
                                       flags.hasAny(VisitorEnum::SECOND_ORDER,
                                                    VisitorEnum::HIGHEST_DERIVATIVES,
                                                    VisitorEnum::ALL_BUFFERS));
            return std::make_unique<Holder>(cv, cdv, cd2v);
        }

        virtual void swap(PlaceHolder* other, Flags<VisitorEnum> flags) override {
            ASSERT((dynamic_cast<Holder<TValue, OrderEnum::SECOND_ORDER>*>(other)));
            auto holder = static_cast<Holder<TValue, OrderEnum::SECOND_ORDER>*>(other);
            this->conditionalSwap(this->v,
                                  holder->v,
                                  flags.hasAny(VisitorEnum::ZERO_ORDER, VisitorEnum::ALL_BUFFERS));
            this->conditionalSwap(this->dv,
                                  holder->dv,
                                  flags.hasAny(VisitorEnum::FIRST_ORDER, VisitorEnum::ALL_BUFFERS));
            this->conditionalSwap(d2v,
                                  holder->d2v,
                                  flags.hasAny(VisitorEnum::SECOND_ORDER,
                                               VisitorEnum::HIGHEST_DERIVATIVES,
                                               VisitorEnum::ALL_BUFFERS));
        }

        virtual Array<LimitedArray<TValue>&> getBuffers() override {
            return { this->v, this->dv, this->d2v };
        }

        LimitedArray<TValue>& get2ndDerivative() { return d2v; }
    };
}


/// Generic container for storing scalar, vector or tensor quantities. Contains current values of the quantity
/// and all derivatives (if there is an evolution equation for the quantity, of course).
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
    QuantityKey idx;

public:
    Quantity() = default;

    Quantity(Quantity&& other) {
        std::swap(data, other.data);
        std::swap(idx, other.idx);
    }

    /// Creates a quantity given number of particles and default value of the quantity. All values are set to
    /// the default value. If the type is 1st-order or 2nd-order, derivatives arrays resized to the same size
    /// as the array of values and set to zero.
    /// \param key Unique identifier of the quantity, used to locate quantity within a storage.
    /// \param size Size of the array, equal to the number of particles.
    /// \param defaultValue Value assigned to all particles.
    /// \param range Optional parameter, used to set bounds for the quantity. By default, quantity is
    ///              unbounded.
    template <typename TValue, OrderEnum TOrder>
    void emplace(QuantityKey key, const int size, const TValue& defaultValue, const Optional<Range> range) {
        using Holder = Detail::Holder<TValue, TOrder>;
        data         = std::make_unique<Holder>(size, defaultValue, range);
        idx          = key;
    }

    /// Creates a quantity using functor that returns an array of values. Can also be directly another Array
    /// or LimitedArray, provided the type of the array is the same. If the type is 1st-order or
    /// 2nd-order, derivatives arrays resized to the same size as the array of values and set to zero.
    template <typename TValue, OrderEnum TOrder, typename TDistribution>
    void emplace(QuantityKey key, TDistribution&& distribution, const Optional<Range> range) {
        using Holder = Detail::Holder<TValue, TOrder>;
        data         = std::make_unique<Holder>(std::forward<TDistribution>(distribution), range);
        idx          = key;
    }

    Quantity& operator=(Quantity&& other) {
        std::swap(data, other.data);
        std::swap(idx, other.idx);
        return *this;
    }

    OrderEnum getOrderEnum() const {
        ASSERT(data);
        return data->getOrderEnum();
    }

    ValueEnum getValueEnum() const {
        ASSERT(data);
        return data->getValueEnum();
    }

    QuantityKey getKey() const { return idx; }

    Quantity clone(const Flags<VisitorEnum> flags) const {
        ASSERT(data);
        Quantity cloned;
        cloned.data = this->data->clone(flags);
        cloned.idx  = this->idx;
        return cloned;
    }

    void swap(Quantity& other, const Flags<VisitorEnum> flags) {
        ASSERT(data);
        this->data->swap(other.data.get(), flags);
        std::swap(this->idx, other.idx);
    }

    /// Casts quantity to given type and temporal enum and returns the holder if the quantity is indeed of
    /// given type and temporal enum, otherwise returns NOTHING. This CANNOT be used to check if the
    /// quantity
    /// is const or 1st order, as even 2nd order quantities can be successfully casted to const or 1st
    /// order.
    template <typename TValue, OrderEnum TOrder>
    Optional<Detail::Holder<TValue, TOrder>&> cast() {
        using Holder   = Detail::Holder<TValue, TOrder>;
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


namespace QuantityCast {
    template <typename TValue>
    Optional<LimitedArray<TValue>&> get(Quantity& quantity) {
        auto holder = quantity.template cast<TValue, OrderEnum::ZERO_ORDER>();
        if (!holder) {
            return NOTHING;
        } else {
            return holder->getValue();
        }
    }

    template <typename TValue>
    Optional<LimitedArray<TValue>&> dt(Quantity& quantity) {
        auto holder = quantity.template cast<TValue, OrderEnum::FIRST_ORDER>();
        if (!holder) {
            return NOTHING;
        } else {
            return holder->getDerivative();
        }
    }

    template <typename TValue>
    Optional<LimitedArray<TValue>&> dt2(Quantity& quantity) {
        auto holder = quantity.template cast<TValue, OrderEnum::SECOND_ORDER>();
        if (!holder) {
            return NOTHING;
        } else {
            return holder->get2ndDerivative();
        }
    }
}

NAMESPACE_SPH_END
