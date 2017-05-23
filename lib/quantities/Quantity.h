#pragma once

/// \file Quantity.h
/// \brief Holder of quantity values and their temporal derivatives
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/containers/Array.h"
#include "objects/wrappers/Flags.h"
#include "objects/wrappers/Variant.h"
#include "quantities/QuantityHelpers.h"

NAMESPACE_SPH_BEGIN


enum class OrderEnum {
    ZERO,   ///< Quantity without derivatives, or "zero order" of quantity
    FIRST,  ///< Quantity with 1st derivative
    SECOND, ///< Quantity with 1st and 2nd derivative
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

    /// Iterates over physical values of quantities. If the quantity does not have physical quantity set (by
    /// material or other object), the stored value is used instead.
    PHYSICAL_VALUES = 1 << 7,
};


namespace Detail {
    /// Abstract holder of all data associated with a quantity. Provides interface to extract information
    /// about the quantity. Must be static/dynamic_casted to one of derived types to extract information
    /// abount stored types or number of stored arrays.
    template <typename TValue>
    class Holder {
    private:
        Array<TValue> v;       /// stored values
        Array<TValue> pv;      /// physical values
        Array<TValue> dv_dt;   /// 1st derivative
        Array<TValue> d2v_dt2; /// 2nd derivative

        OrderEnum order;

        Holder(const OrderEnum order)
            : order(order) {}

    public:
        Holder(const OrderEnum order, const TValue& defaultValue, const Size size)
            : order(order) {
            v.resize(size);
            v.fill(defaultValue);
            initDerivatives(size);
        }

        Holder(const OrderEnum order, Array<TValue>&& values)
            : v(std::move(values))
            , order(order) {
            initDerivatives(v.size());
        }

        /// Returns number of derivatives stored within the quantity
        INLINE OrderEnum getOrderEnum() const {
            return order;
        }

        /// Return type of quantity values
        INLINE ValueEnum getValueEnum() const {
            return GetValueEnum<TValue>::type;
        }

        /// Returns size of the stored arrays (=number of particles).
        INLINE Size size() const {
            return v.size();
        }

        INLINE Array<TValue>& getValue() {
            return v;
        }

        INLINE const Array<TValue>& getValue() const {
            return v;
        }

        INLINE Array<TValue>& getPhysicalValue() {
            return pv.empty() ? v : pv;
        }

        INLINE const Array<TValue>& getPhysicalValue() const {
            return pv.empty() ? v : pv;
        }

        INLINE Array<TValue>& getDt() {
            ASSERT(order == OrderEnum::FIRST || order == OrderEnum::SECOND);
            return dv_dt;
        }

        INLINE const Array<TValue>& getDt() const {
            ASSERT(order == OrderEnum::FIRST || order == OrderEnum::SECOND);
            return dv_dt;
        }

        INLINE Array<TValue>& getD2t() {
            ASSERT(order == OrderEnum::SECOND);
            return d2v_dt2;
        }

        INLINE const Array<TValue>& getD2t() const {
            ASSERT(order == OrderEnum::SECOND);
            return d2v_dt2;
        }

        INLINE StaticArray<Array<TValue>&, 3> getAll() {
            switch (order) {
            case OrderEnum::ZERO:
                return { v };
            case OrderEnum::FIRST:
                return { v, dv_dt };
            case OrderEnum::SECOND:
                return { v, dv_dt, d2v_dt2 };
            default:
                NOT_IMPLEMENTED;
            }
        }

        INLINE StaticArray<const Array<TValue>&, 3> getAll() const {
            switch (order) {
            case OrderEnum::ZERO:
                return { v };
            case OrderEnum::FIRST:
                return { v, dv_dt };
            case OrderEnum::SECOND:
                return { v, dv_dt, d2v_dt2 };
            default:
                NOT_IMPLEMENTED;
            }
        }

        INLINE StaticArray<Array<TValue>&, 3> getPhysicalAll() {
            switch (order) {
            case OrderEnum::ZERO:
                return { getPhysicalValue() };
            case OrderEnum::FIRST:
                return { getPhysicalValue(), dv_dt };
            case OrderEnum::SECOND:
                return { getPhysicalValue(), dv_dt, d2v_dt2 };
            default:
                NOT_IMPLEMENTED;
            }
        }

        INLINE StaticArray<Array<TValue>&, 2> modify() {
            if (pv.size() != v.size()) {
                // lazy initialization
                pv = copyable(v);
            }
            return { v, pv };
        }

        /// Clones the quantity, optionally selecting arrays to clone; returns them as unique_ptr.
        Holder clone(const Flags<VisitorEnum> flags) const {
            Holder cloned(order);
            visitConst(cloned, flags, [](const Array<TValue>& array, Array<TValue>& clonedArray) {
                clonedArray = array.clone();
            });
            return cloned;
        }

        /// Swaps arrays in two quantities, optionally selecting arrays to swap.
        void swap(Holder& other, Flags<VisitorEnum> flags) {
            visitMutable(other, flags, [](Array<TValue>& ar1, Array<TValue>& ar2) { ar1.swap(ar2); });
        }

        /// Changes order
        void setOrder(const OrderEnum newOrder) {
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

    private:
        void initDerivatives(const Size size) {
            switch (order) {
            case OrderEnum::SECOND:
                d2v_dt2.resize(size);
                d2v_dt2.fill(TValue(0._f));
                FALLTHROUGH;
            case OrderEnum::FIRST:
                dv_dt.resize(size);
                dv_dt.fill(TValue(0._f));
                break;
            default:
                break;
            }
        }

        template <typename TFunctor>
        void visitMutable(Holder& other, const Flags<VisitorEnum> flags, TFunctor&& functor) {
            if (flags.hasAny(VisitorEnum::ZERO_ORDER,
                    VisitorEnum::ALL_BUFFERS,
                    VisitorEnum::ALL_VALUES,
                    VisitorEnum::DEPENDENT_VALUES)) {
                functor(v, other.v);
            }
            if (flags.hasAny(VisitorEnum::ALL_BUFFERS, VisitorEnum::PHYSICAL_VALUES)) {
                functor(pv, other.pv);
            }
            switch (order) {
            case OrderEnum::FIRST:
                if (flags.hasAny(VisitorEnum::FIRST_ORDER,
                        VisitorEnum::ALL_BUFFERS,
                        VisitorEnum::HIGHEST_DERIVATIVES)) {
                    functor(dv_dt, other.dv_dt);
                }
                break;
            case OrderEnum::SECOND:
                if (flags.hasAny(VisitorEnum::ALL_BUFFERS, VisitorEnum::DEPENDENT_VALUES)) {
                    functor(dv_dt, other.dv_dt);
                }
                if (flags.hasAny(VisitorEnum::ALL_BUFFERS,
                        VisitorEnum::SECOND_ORDER,
                        VisitorEnum::HIGHEST_DERIVATIVES)) {
                    functor(d2v_dt2, other.d2v_dt2);
                }
                break;
            default:
                break;
            }
        }

        template <typename TFunctor>
        void visitConst(Holder& other, const Flags<VisitorEnum> flags, TFunctor&& functor) const {
            const_cast<Holder*>(this)->visitMutable(other, flags, std::forward<TFunctor>(functor));
        }
    };
}

/// \todo update documentation
/// Generic container for storing scalar, vector or tensor quantities. Contains current values of the quantity
/// and all derivatives (if there is an evolution equation for the quantity, of course).
/// As the quantity can have data of different types, there is no direct way to access the arrays stored
/// within. There are several methods, however, that allow to access the information indirectly:
/// 1) cast<Type, TemporalEnum>; returns holder of quantity (see above) IF the type and temporal enum in
///                              template parameters match the ones of the holder.
/// 2) getAll<Type>; returns all arrays (values and derivatives) stored in the holder IF the template type
///                      matches the holder type.
/// Beside accessing values through cast<> method, you can also use functions in QuantityCast namespace that
/// allow to access values or given derivative. The system is the same, though; we try to get
/// values/derivatives of given type and if they are stored within the quantity, they are returned.
class Quantity : public Noncopyable {
private:
    template <typename... TArgs>
    using HolderVariant = Variant<NothingType, Detail::Holder<TArgs>...>;

    // Types must be in same order as in ValueEnum!
    using Holder = HolderVariant<Float, Vector, Tensor, SymmetricTensor, TracelessTensor, Size>;
    Holder data;

    Quantity(Holder&& holder)
        : data(std::move(holder)) {}

public:
    /// Constructs an empty quantity. Calling any member functions will cause assert, quantity can be then
    /// created using move constructor. The default constructor allows using Quantity in STL containers.
    Quantity() = default;

    /// Creates a quantity given number of particles and default value of the quantity. All values are set to
    /// the default value. If the type is 1st-order or 2nd-order, derivatives arrays resized to the same size
    /// as the array of values and set to zero.
    /// \param defaultValue Value assigned to all particles.
    /// \param size Size of the array, equal to the number of particles.
    template <typename TValue>
    Quantity(const OrderEnum order, const TValue& defaultValue, const Size size)
        : data(Detail::Holder<TValue>(order, defaultValue, size)) {}

    /// Creates a quantity from an array of values. All derivatives are set to zero.
    template <typename TValue>
    Quantity(const OrderEnum order, Array<TValue>&& values)
        : data(Detail::Holder<TValue>(order, std::move(values))) {}

    OrderEnum getOrderEnum() const {
        return forValue(data, [](auto& holder) INL { return holder.getOrderEnum(); });
    }

    ValueEnum getValueEnum() const {
        ASSERT(data.getTypeIdx() != 0);
        return ValueEnum(data.getTypeIdx() - 1);
    }

    Quantity clone(const Flags<VisitorEnum> flags) const {
        Holder cloned = forValue(data, [flags](auto& holder) -> Holder { return holder.clone(flags); });
        return cloned;
    }

    /// Swap quantity (or selected part of it) with other quantity.
    void swap(Quantity& other, const Flags<VisitorEnum> flags) {
        ASSERT(this->getValueEnum() == other.getValueEnum());
        forValue(data, [flags, &other](auto& holder) {
            using Type = std::decay_t<decltype(holder)>;
            return holder.swap(other.data.get<Type>(), flags);
        });
    }

    /// Returns the size of the quantity (number of particles)
    INLINE Size size() const {
        return forValue(data, [](auto& holder) INL { return holder.size(); });
    }

    /// Returns a reference to array of quantity values. The type of the quantity must match the provided
    /// type, checked by assert. To check whether the type of the quantity match, use getValueEnum().
    template <typename TValue>
    INLINE Array<TValue>& getValue() {
        return get<TValue>().getValue();
    }

    /// Returns a reference to array of quantity values, const version.
    template <typename TValue>
    INLINE const Array<TValue>& getValue() const {
        return get<TValue>().getValue();
    }

    /// Returns a reference to array of physical values. If there is no modification to the quantity, simply
    /// returns stored values.
    template <typename TValue>
    INLINE Array<TValue>& getPhysicalValue() {
        return get<TValue>().getPhysicalValue();
    }

    /// /// Returns a reference to array of physical values, const version.
    template <typename TValue>
    INLINE const Array<TValue>& getPhysicalValue() const {
        return get<TValue>().getPhysicalValue();
    }

    template <typename TValue>
    INLINE StaticArray<Array<TValue>&, 2> modify() {
        return get<TValue>().modify();
    }

    void setOrder(const OrderEnum order) {
        return forValue(data, [order](auto& holder) INL { return holder.setOrder(order); });
    }

    /// Returns a reference to array of first derivatives of quantity. The type of the quantity must match the
    /// provided type and the quantity must be (at least) 1st order, checked by assert. To check whether the
    /// type and order match, use getValueEnum() and getOrderEnum(), respectively.
    template <typename TValue>
    INLINE Array<TValue>& getDt() {
        return get<TValue>().getDt();
    }

    /// Returns a reference to array of first derivatives of quantity, const version.
    template <typename TValue>
    INLINE const Array<TValue>& getDt() const {
        return get<TValue>().getDt();
    }

    /// Returns a reference to array of second derivatives of quantity. The type of the quantity must match
    /// the provided type and the quantity must be 2st order, checked by assert. To check whether the type and
    /// order match, use getValueEnum() and getOrderEnum(), respectively.
    template <typename TValue>
    INLINE Array<TValue>& getD2t() {
        return get<TValue>().getD2t();
    }

    /// Returns a reference to array of second derivatives of quantity, const version.
    template <typename TValue>
    INLINE const Array<TValue>& getD2t() const {
        return get<TValue>().getD2t();
    }

    /// Returns all buffers of given type stored in this quantity. If the quantity is of different type, an
    /// empty array is returned. Buffers in array are ordered such that quantity values is the first element
    /// (zero index), first derivative is the second element etc.
    template <typename TValue>
    StaticArray<Array<TValue>&, 3> getAll() {
        return get<TValue>().getAll();
    }

    template <typename TValue>
    StaticArray<const Array<TValue>&, 3> getAll() const {
        return get<TValue>().getAll();
    }

    template <typename TValue>
    StaticArray<Array<TValue>&, 3> getPhysicalAll() {
        return get<TValue>().getPhysicalAll();
    }

    /// Iterates through the quantity values using given integer sequence and clamps the visited values to
    /// given range.
    /// \todo clamp derivatives as well
    template <typename TIndexSequence>
    void clamp(const TIndexSequence& sequence, const Range range) {
        forValue(data, [&sequence, range](auto& v) {
            auto& values = v.getValue();
            for (Size idx : sequence) {
                values[idx] = Sph::clamp(values[idx], range);
            }
        });
    }

private:
    template <typename TValue>
    Detail::Holder<TValue>& get() {
        return data.get<Detail::Holder<TValue>>();
    }

    template <typename TValue>
    const Detail::Holder<TValue>& get() const {
        return data.get<Detail::Holder<TValue>>();
    }
};

NAMESPACE_SPH_END
