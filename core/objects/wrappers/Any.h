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
    private:
        Type value;

    public:
        Holder(const Type& value)
            : value(value) {}
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
    auto* cast() {
        using RawT = std::decay_t<Type>;
        return dynamic_cast<Holder<RawT>*>(&*data);
    }

    template <typename Type>
    const auto* cast() const {
        using RawT = std::decay_t<Type>;
        return dynamic_cast<const Holder<RawT>*>(&*data);
    }

public:
    Any() = default;

    /// Constructs Any from given value, type is deduced.
    template <typename Type>
    Any(Type&& value) {
        data = makeAuto<Holder<Type>>(std::forward<Type>(value));
    }

    /// Tries to extract value of given type from Any. On success returns the value, otherwise NOTHING.
    template <typename Type>
    friend Optional<Type> anyCast(const Any& any);

    /// Compares Any with another value. Returns true if and only if both types and values are equal.
    template <typename Type>
    bool operator==(Type&& value) const {
        auto casted = cast<Type>();
        return casted && casted->get() == value;
    }

    template <typename Type>
    bool operator!=(Type&& value) const {
        return !(*this == std::forward<Type>(value));
    }
};

template <typename Type>
Optional<Type> anyCast(const Any& any) {
    const Any::Holder<Type>* casted = any.cast<Type>();
    if (casted == nullptr) {
        // either nothing stored or different type
        return NOTHING;
    }
    return casted->get();
}


NAMESPACE_SPH_END
