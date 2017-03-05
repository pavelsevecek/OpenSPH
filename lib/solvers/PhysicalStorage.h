#pragma once

#include "quantities/Storage.h"
#include <memory>

/// Allows to modify physical quantities while keeping original (unchanged) values in storage.
/// Can be used for physical model (damage modifying stress tensor), or to store different values than want to
/// work with (for example storing difference in densities (rho - rho_0) rather than density itself)
/// For quantities that cannot be modified in place.
/// Only one modifier can modify one quantity.
///
/// currently can only modify values, not derivatives
///
/// One modifier can modify more quantities

/// initialize
///  sum
/// finalize
///
/// initial conditions: create !

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Modifier : public Polymorphic {
    public:
        /// Modify the quantity, called once before the solver loop.
        virtual void initialize(Storage& storage) = 0;

        virtual bool modifies(const QuantityIds key) const = 0;

        /// Returns the modified value of the quantity
        template <typename TValue>
        Array<TValue>& getModifiedValue(const QuantityIds& id) {
            Quantity& modified = modify(id);
            return modified.getValue<TValue>();
        }

    private:
        /// Implementation shall return zero-order quantity, containing modified values.
        virtual Quantity& modify(const QuantityIds key) = 0;
    };
}

/// Storage holder providing access to physical values of quantities (after applying all modifiers).
/// If no modifier is used, this simply forwards calls to storage without changing the values.
/// All derivatives are always returned unchanged
class PhysicalStorage {
private:
    std::shared_ptr<Storage> storage;
    Array<std::unique_ptr<Abstract::Modifier>> modifiers;


    operator->forwarding to storage !public :
        /// Called in the constructor of solver
        void
        addModifier(std::unique_ptr<Abstract::Modifier>&& modifier) {
#ifdef DEBUG
        // check we are not already modifying the quantity
        QuantityIds id = modifier->modifies();
        for (auto& m : modifiers) {
            ASSERT(!m.modifies(key));
        }
#endif
        modifiers.push(std::move(modifier));
    }

    /// Called before each loop
    void initialize(const std::shared_ptr<Storage>& newStorage) {
        storage = newStorage;
        for (auto& m : modifiers) {
            m->initialize(*storage);
        }
    }

    template <typename TValue>
    Array<TValue>& getValue(const QuantityIds& key) {
        for (auto& m : modifiers) {
            if (m.modifies(key)) {
                return m->getModifiedValue();
            }
        }
        // no modifiers => return stored value
        return storage->getValue<TValue>(key);
    }

    template <typename TValue, typename... TArgs>
    auto getValues(const QuantityIds first, const QuantityIds second, const TArgs... others) {
        return tie(getValue<TValue>(first), getValue<TValue>(second), getValue<TValue>(others)...);
    }

    template <typename TValue>
    StaticArray<Array<TValue>&, 3> getAll(const QuantityIds key) {
        StaticArray<Array<TValue>&, 3> buffers = storage->getAll<TValue>(key);
        // replace the value with modified value
        switch (buffers.size()) {
        case 1:
            return { this->getValue(key) };
        case 2:
            return { this->getValue(key), buffers[1] };
        case 3:
            return { this->getValue(key), buffers[1], buffers[2] };
        default:
            STOP;
        }
    }
};

NAMESPACE_SPH_END
