#pragma once

#include "objects/containers/Array.h"
#include "objects/wrappers/Flags.h"
#include "objects/wrappers/Optional.h"
#include "quantities/QuantityHelpers.h"

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

    /// Iterate over quantity values for 1st order quantities and over values and 1st derivatives of 2nd order
    /// quantities. Zero order quantities are skipped.
    DEPENDENT_VALUES = 1 << 5,

    /// Iterates over all 1st order and 2nd order quantities, passes their 1st and 2nd derivatives as
    /// parameters, respectively.
    HIGHEST_DERIVATIVES = 1 << 6,
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

        /// Clamps values by the definition range. Unbounded values are unaffected, as well as all
        /// derivatives.
        virtual void clamp() = 0;

        /// \todo remote together with clamp
        virtual Range getRange() const = 0;

        /// Returns size of the stored arrays (=number of particles).
        virtual int size() const = 0;
    };

    /// Abstract extension of PlaceHolder that adds interface to get all buffers of given type stored in the
    /// holder.
    template <typename TValue>
    struct ValueHolder : public PlaceHolder {
        virtual StaticArray<Array<TValue>&, 3> getBuffers() = 0;
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
        Array<TValue> v;
        Range range;

        Array<TValue> conditionalClone(const Array<TValue>& array, const bool condition) const {
            if (condition) {
                return array.clone();
            } else {
                return Array<TValue>();
            }
        }

        void conditionalSwap(Array<TValue>& ar1, Array<TValue>& ar2, const bool condition) const {
            if (condition) {
                ar1.swap(ar2);
            }
        }

    public:
        Holder() = default;

        /// Move constructor
        Holder(Array<TValue>&& v, const Range range = Range::unbounded())
            : v(std::move(v))
            , range(range) {}

        Holder(const TValue& value, const int size, const Range range = Range::unbounded())
            : range(range) {
            v.resize(size);
            v.fill(value);
        }

        virtual OrderEnum getOrderEnum() const override {
            return OrderEnum::ZERO_ORDER;
        }

        virtual ValueEnum getValueEnum() const override {
            return GetValueEnum<TValue>::type;
        }

        virtual std::unique_ptr<PlaceHolder> clone(const Flags<VisitorEnum> flags) const override {
            Array<TValue> cv =
                conditionalClone(v, flags.hasAny(VisitorEnum::ZERO_ORDER, VisitorEnum::ALL_BUFFERS));
            return std::make_unique<Holder>(std::move(cv), range);
        }

        virtual void clamp() override {
            if (range == Range::unbounded()) {
                return;
            }
            for (TValue& x : v) {
                x = Sph::clamp(x, range);
            }
        }

        virtual Range getRange() const override {
            return range;
        }

        virtual void swap(PlaceHolder* other, Flags<VisitorEnum> flags) override {
            ASSERT((dynamic_cast<Holder<TValue, OrderEnum::ZERO_ORDER>*>(other)));
            auto holder = static_cast<Holder<TValue, OrderEnum::ZERO_ORDER>*>(other);
            conditionalSwap(v, holder->v, flags.hasAny(VisitorEnum::ZERO_ORDER, VisitorEnum::ALL_BUFFERS));
        }

        virtual int size() const override {
            return v.size();
        }

        virtual StaticArray<Array<TValue>&, 3> getBuffers() override {
            return { this->v };
        }

        Array<TValue>& getValue() {
            return v;
        }
    };

    /// Holder for first-order quantities, contains also derivative of the quantity. The derivative can be
    /// accessed using getDerivative() method.
    template <typename TValue>
    class Holder<TValue, OrderEnum::FIRST_ORDER> : public Holder<TValue, OrderEnum::ZERO_ORDER> {
    protected:
        Array<TValue> dv;

    public:
        Holder() = default;

        Holder(Array<TValue>&& v, Array<TValue>&& dv)
            : Holder<TValue, OrderEnum::ZERO_ORDER>(std::move(v))
            , dv(std::move(dv)) {}

        Holder(const TValue& value, const int size, const Range range = Range::unbounded())
            : Holder<TValue, OrderEnum::ZERO_ORDER>(value, size, range) {
            dv.resize(this->v.size());
            dv.fill(TValue(0._f));
        }

        Holder(Array<TValue>&& values, const Range range = Range::unbounded())
            : Holder<TValue, OrderEnum::ZERO_ORDER>(std::move(values), range) {
            dv.resize(this->v.size());
            dv.fill(TValue(0._f));
        }

        virtual OrderEnum getOrderEnum() const override {
            return OrderEnum::FIRST_ORDER;
        }

        virtual ValueEnum getValueEnum() const override {
            return GetValueEnum<TValue>::type;
        }

        /*virtual void clamp() override {
            if (this->range == Range::unbounded()) {
                return;
            }
            for (Size i = 0; i < this->v.size(); ++i) {
                tie(this->v[i], this->dv[i]) = clampWithDerivative(this->v[i], this->dv[i], this->range);
            }
        }*/

        virtual std::unique_ptr<PlaceHolder> clone(const Flags<VisitorEnum> flags) const override {
            Array<TValue> cv = this->conditionalClone(
                this->v, flags.hasAny(VisitorEnum::ZERO_ORDER, VisitorEnum::ALL_BUFFERS));
            Array<TValue> cdv = this->conditionalClone(this->dv,
                flags.hasAny(
                    VisitorEnum::FIRST_ORDER, VisitorEnum::HIGHEST_DERIVATIVES, VisitorEnum::ALL_BUFFERS));
            return std::make_unique<Holder>(std::move(cv), std::move(cdv));
        }

        virtual void swap(PlaceHolder* other, Flags<VisitorEnum> flags) override {
            ASSERT((dynamic_cast<Holder<TValue, OrderEnum::FIRST_ORDER>*>(other)));
            auto holder = static_cast<Holder<TValue, OrderEnum::FIRST_ORDER>*>(other);
            this->conditionalSwap(this->v,
                holder->v,
                flags.hasAny(
                    VisitorEnum::ZERO_ORDER, VisitorEnum::ALL_BUFFERS, VisitorEnum::DEPENDENT_VALUES));
            this->conditionalSwap(this->dv,
                holder->dv,
                flags.hasAny(
                    VisitorEnum::FIRST_ORDER, VisitorEnum::HIGHEST_DERIVATIVES, VisitorEnum::ALL_BUFFERS));
        }

        virtual StaticArray<Array<TValue>&, 3> getBuffers() override {
            return { this->v, this->dv };
        }

        Array<TValue>& getDerivative() {
            return dv;
        }
    };

    /// Holder for second-order quantities, contains also second derivative of the quantity. The second
    /// derivative can be accessed using get2ndDerivative() method.
    template <typename TValue>
    class Holder<TValue, OrderEnum::SECOND_ORDER> : public Holder<TValue, OrderEnum::FIRST_ORDER> {
    private:
        Array<TValue> d2v;

    public:
        Holder() = default;

        Holder(Array<TValue>&& v, Array<TValue>&& dv, Array<TValue>&& d2v)
            : Holder<TValue, OrderEnum::FIRST_ORDER>(std::move(v), std::move(dv))
            , d2v(std::move(d2v)) {}

        Holder(const TValue& value, const int size, const Range range = Range::unbounded())
            : Holder<TValue, OrderEnum::FIRST_ORDER>(value, size, range) {
            d2v.resize(this->v.size());
            d2v.fill(TValue(0._f));
        }

        Holder(Array<TValue>&& values, const Range range = Range::unbounded())
            : Holder<TValue, OrderEnum::FIRST_ORDER>(std::move(values), range) {
            d2v.resize(this->v.size());
            d2v.fill(TValue(0._f));
        }

        virtual OrderEnum getOrderEnum() const final {
            return OrderEnum::SECOND_ORDER;
        }

        virtual ValueEnum getValueEnum() const final {
            return GetValueEnum<TValue>::type;
        }

        /*virtual void clamp() override {
            if (this->range == Range::unbounded()) {
                return;
            }
            for (Size i = 0; i < this->v.size(); ++i) {
                //tie(this->v[i], this->d2v[i]) = clampWithDerivative(this->v[i], this->d2v[i], this->range);
                this
            }
        }*/

        virtual std::unique_ptr<PlaceHolder> clone(const Flags<VisitorEnum> flags) const override {
            Array<TValue> cv = this->conditionalClone(
                this->v, flags.hasAny(VisitorEnum::ZERO_ORDER, VisitorEnum::ALL_BUFFERS));
            Array<TValue> cdv = this->conditionalClone(
                this->dv, flags.hasAny(VisitorEnum::FIRST_ORDER, VisitorEnum::ALL_BUFFERS));
            Array<TValue> cd2v = this->conditionalClone(d2v,
                flags.hasAny(
                    VisitorEnum::SECOND_ORDER, VisitorEnum::HIGHEST_DERIVATIVES, VisitorEnum::ALL_BUFFERS));
            return std::make_unique<Holder>(std::move(cv), std::move(cdv), std::move(cd2v));
        }

        virtual void swap(PlaceHolder* other, Flags<VisitorEnum> flags) override {
            ASSERT((dynamic_cast<Holder<TValue, OrderEnum::SECOND_ORDER>*>(other)));
            auto holder = static_cast<Holder<TValue, OrderEnum::SECOND_ORDER>*>(other);
            this->conditionalSwap(this->v,
                holder->v,
                flags.hasAny(
                    VisitorEnum::ZERO_ORDER, VisitorEnum::ALL_BUFFERS, VisitorEnum::DEPENDENT_VALUES));
            this->conditionalSwap(this->dv,
                holder->dv,
                flags.hasAny(
                    VisitorEnum::FIRST_ORDER, VisitorEnum::ALL_BUFFERS, VisitorEnum::DEPENDENT_VALUES));
            this->conditionalSwap(d2v,
                holder->d2v,
                flags.hasAny(
                    VisitorEnum::SECOND_ORDER, VisitorEnum::HIGHEST_DERIVATIVES, VisitorEnum::ALL_BUFFERS));
        }

        virtual StaticArray<Array<TValue>&, 3> getBuffers() override {
            return { this->v, this->dv, this->d2v };
        }

        Array<TValue>& get2ndDerivative() {
            return d2v;
        }
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

public:
    Quantity() = default;

    Quantity(Quantity&& other) {
        std::swap(data, other.data);
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
    void insert(const TValue& defaultValue, const int size, const Range& range = Range::unbounded()) {
        using Holder = Detail::Holder<TValue, TOrder>;
        data = std::make_unique<Holder>(defaultValue, size, range);
    }

    /// Creates a quantity from an array of values. All derivatives are set to zero.
    template <typename TValue, OrderEnum TOrder>
    void insert(Array<TValue>&& values, const Range& range = Range::unbounded()) {
        using Holder = Detail::Holder<TValue, TOrder>;
        data = std::make_unique<Holder>(std::move(values), range);
    }

    Quantity& operator=(Quantity&& other) {
        std::swap(data, other.data);
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

    Quantity clone(const Flags<VisitorEnum> flags) const {
        ASSERT(data);
        Quantity cloned;
        cloned.data = this->data->clone(flags);
        return cloned;
    }

    /// Clamps quantity values by the range. Derivatives are unchanged.
    void clamp() {
        ASSERT(data);
        data->clamp();
    }

    Range getRange() const {
        ASSERT(data);
        return data->getRange();
    }

    /// Swap quantity (or selected part of it) with other quantity.
    void swap(Quantity& other, const Flags<VisitorEnum> flags) {
        ASSERT(data);
        data->swap(other.data.get(), flags);
    }

    /// Returns the size of the quantity (number of particles)
    Size size() const {
        if (!data) {
            return 0;
        }
        return data->size();
    }

    /// Returns a reference to array of quantity values. The type of the quantity must match the provided
    /// type, checked by assert. To check whether the type of the quantity match, use getValueEnum().
    template <typename TValue>
    Array<TValue>& getValue() {
        auto& holder = cast<TValue, OrderEnum::ZERO_ORDER>();
        return holder.getValue();
    }

    /// Returns a reference to array of quantity values, const version.
    template <typename TValue>
    const Array<TValue>& getValue() const {
        const auto& holder = cast<TValue, OrderEnum::ZERO_ORDER>();
        return holder.getValue();
    }

    /// Returns a reference to array of first derivatives of quantity. The type of the quantity must match the
    /// provided type and the quantity must be (at least) 1st order, checked by assert. To check whether the
    /// type and order match, use getValueEnum() and getOrderEnum(), respectively.
    template <typename TValue>
    Array<TValue>& getDt() {
        auto& holder = cast<TValue, OrderEnum::FIRST_ORDER>();
        return holder.getDerivative();
    }

    /// Returns a reference to array of first derivatives of quantity, const version.
    template <typename TValue>
    const Array<TValue>& getDt() const {
        const auto& holder = cast<TValue, OrderEnum::FIRST_ORDER>();
        return holder.getDerivative();
    }

    /// Returns a reference to array of second derivatives of quantity. The type of the quantity must match
    /// the provided type and the quantity must be 2st order, checked by assert. To check whether the type and
    /// order match, use getValueEnum() and getOrderEnum(), respectively.
    template <typename TValue>
    Array<TValue>& getD2t() {
        auto& holder = cast<TValue, OrderEnum::SECOND_ORDER>();
        return holder.get2ndDerivative();
    }

    /// Returns a reference to array of second derivatives of quantity, const version.
    template <typename TValue>
    const Array<TValue>& getD2t() const {
        const auto& holder = cast<TValue, OrderEnum::SECOND_ORDER>();
        return holder.get2ndDerivative();
    }

    /// Returns all buffers of given type stored in this quantity. If the quantity is of different type, an
    /// empty array is returned. Buffers in array are ordered such that quantity values is the first element
    /// (zero index), first derivative is the second element etc.
    template <typename TValue>
    StaticArray<Array<TValue>&, 3> getBuffers() {
        using Holder = Detail::ValueHolder<TValue>;
        Holder* holder = dynamic_cast<Holder*>(data.get());
        if (holder) {
            return holder->getBuffers();
        } else {
            return EMPTY_ARRAY;
        }
    }

private:
    template <typename TValue, OrderEnum TOrder>
    Detail::Holder<TValue, TOrder>& cast() {
        using Holder = Detail::Holder<TValue, TOrder>;
        Holder* holder = dynamic_cast<Holder*>(data.get());
        ASSERT(holder);
        return *holder;
    }

    template <typename TValue, OrderEnum TOrder>
    const Detail::Holder<TValue, TOrder>& cast() const {
        using Holder = Detail::Holder<TValue, TOrder>;
        const Holder* holder = dynamic_cast<const Holder*>(data.get());
        ASSERT(holder);
        return *holder;
    }
};

NAMESPACE_SPH_END
