#pragma once

/// \file Any.h
/// \brief Object that can store value of any type
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/wrappers/AutoPtr.h"
#include "objects/wrappers/Optional.h"
#include <typeinfo>

NAMESPACE_SPH_BEGIN

/// Type-safe object that can store value of any type, similar to std::any.
class Any {
private:
    class AbstractHolder : public Polymorphic {
    public:
        virtual const std::type_info& getTypeInfo() const = 0;
        virtual AutoPtr<AbstractHolder> clone() const = 0;
    };

    template <typename Type>
    class Holder : public AbstractHolder {
        static_assert(!std::is_reference<Type>::value, "Cannot store references in Any");

    private:
        Type value;

    public:
        Holder(const Type& value)
            : value(value) {}

        Holder(Type&& value)
            : value(std::move(value)) {}

        virtual const std::type_info& getTypeInfo() const {
            return typeid(value);
        }

        virtual AutoPtr<AbstractHolder> clone() const {
            return makeAuto<Holder>(value);
        }

        Type& get() {
            return value;
        }

        const Type& get() const {
            return value;
        }
    };

    AutoPtr<AbstractHolder> data;

    template <typename Type>
    using HolderType = Holder<std::decay_t<Type>>;

    template <typename Type>
    HolderType<Type>* safeCast() {
        if (data) {
            return dynamic_cast<HolderType<Type>*>(&*data);
        } else {
            return nullptr;
        }
    }

    template <typename Type>
    const HolderType<Type>* safeCast() const {
        if (data) {
            return dynamic_cast<const HolderType<Type>*>(&*data);
        } else {
            return nullptr;
        }
    }

    template <typename Type>
    HolderType<Type>* cast() {
        SPH_ASSERT(data);
        return assert_cast<HolderType<Type>*>(&*data);
    }

    template <typename Type>
    const HolderType<Type>* cast() const {
        SPH_ASSERT(data);
        return assert_cast<const HolderType<Type>*>(&*data);
    }


public:
    /// Constructs an empty Any.
    Any() = default;

    /// Copies the value from another Any.
    Any(const Any& other) {
        *this = other;
    }

    /// Moves the value from another Any.
    Any(Any&& other) {
        *this = std::move(other);
    }

    /// Constructs Any from given value, type is deduced.
    template <typename Type, typename = std::enable_if_t<!std::is_same<Any, std::decay_t<Type>>::value>>
    Any(Type&& value) {
        *this = std::forward<Type>(value);
    }

    /// Copies the value from another Any.
    Any& operator=(const Any& other) {
        if (other.data) {
            data = other.data->clone();
        } else {
            data = nullptr;
        }
        return *this;
    }

    /// Moves the value from another Any.
    Any& operator=(Any&& other) {
        data = std::move(other.data);
        return *this;
    }

    /// Assigns given value to Any, type is deduced.
    template <typename Type, typename = std::enable_if_t<!std::is_same<Any, std::decay_t<Type>>::value>>
    Any& operator=(Type&& value) {
        data = makeAuto<HolderType<Type>>(std::forward<Type>(value));
        return *this;
    }

    /// \brief Returns the value from Any.
    ///
    /// If Any does not store any value or the value has different type, the result is undefined. In debug
    /// build, it is checked by assert.
    template <typename Type>
    explicit operator const Type&() const {
        return cast<Type>()->get();
    }

    /// \brief Returns the reference to the stored value in Any.
    ///
    /// If Any does not store any value or the value has different type, the result is undefined. In debug
    /// build, it is checked by assert.
    template <typename Type>
    explicit operator Type&() {
        return cast<Type>()->get();
    }

    /// \brief Checks if Any currently holds a values.
    bool hasValue() const {
        return bool(data);
    }

    /// \brief Tries to extract value of given type from Any.
    ///
    /// On success returns the value, otherwise NOTHING.
    template <typename Type>
    friend Optional<Type> anyCast(const Any& any);

    /// \brief Compares Any with another value.
    ///
    /// Returns true if and only if both types and values are equal.
    template <typename Type, typename = std::enable_if_t<!std::is_same<Any, std::decay_t<Type>>::value>>
    bool operator==(Type&& value) const {
        auto casted = safeCast<Type>();
        return casted && casted->get() == value;
    }

    template <typename Type, typename = std::enable_if_t<!std::is_same<Any, std::decay_t<Type>>::value>>
    bool operator!=(Type&& value) const {
        return !(*this == std::forward<Type>(value));
    }
};

template <typename Type>
Optional<Type> anyCast(const Any& any) {
    const Any::Holder<Type>* casted = any.safeCast<Type>();
    if (casted == nullptr) {
        // either nothing stored or different type
        return NOTHING;
    }
    return casted->get();
}

NAMESPACE_SPH_END
