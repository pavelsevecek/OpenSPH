#pragma once

#include "objects/wrappers/Value.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

/// \brief Object holding information about single particle
///
/// \ref Particle can store all or some quantities of given particle. Unlike in \ref Storage, the \ref
/// Particle does not hold information about orders of the stored quantities, and it is possible to store
/// derivatives of quantities without having quantity values stored as well.
class Particle {
private:
    Size idx;

    struct Visitor;

    struct InternalQuantityData {
        Value value;
        Value dt;
        Value d2t;
    };

    std::map<QuantityId, InternalQuantityData> data;

public:
    /// Default constructor, defined only for convenient usage in containers, etc. Particle has to be
    /// initialized using copy/move operator.
    Particle() = default;

    /// Constructs object given its index in parent storage, containing no particle data. Quantity data can be
    /// added using functions \ref addValue, \ref addDt and \ref addD2t.
    Particle(const Size idx)
        : idx(idx) {}

    /// Constructs the object from storage, storing values of all quantities for given particle
    /// \param storage Storage containing the particle
    /// \param idx Index of the particle
    Particle(const Storage& storage, const Size idx);

    /// Constructs a particle with information about a single quantity value. Other quantity data can be added
    /// using functions \ref addValue, \ref addDt and \ref addD2t.
    /// \param id Quantity ID of the quantity value
    /// \param value Quantity value. Cannot be used to add derivatives.
    /// \param idx Index of particle; although this constructor is not necessarily bound to a particle
    ///            storage, this represents index to storage associated with the particle.
    Particle(const QuantityId id, const Value& value, const Size idx);

    /// Adds another quantity value or updates the value of quantity previously stored.
    /// \param id Quantity ID of the quantity value
    /// \param value New value of given quantity
    /// \return Reference to itself, allowing to queue functions
    Particle& addValue(const QuantityId id, const Value& value);

    /// Adds another quantity derivative or updates the derivative of quantity previously stored.
    /// \param id Quantity ID of the quantity value
    /// \param value New derivative of given quantity
    /// \return Reference to itself, allowing to queue functions
    Particle& addDt(const QuantityId id, const Value& value);

    /// Adds another quantity 2nd derivative or updates the 2nd derivative of quantity previously stored.
    /// \param id Quantity ID of the quantity value
    /// \param value New 2nd derivative of given quantity
    /// \return Reference to itself, allowing to queue functions
    Particle& addD2t(const QuantityId id, const Value& value);

    /// Returns the index of particle in the parent storage.
    INLINE Size getIndex() const {
        return idx;
    }

    /// Retrieves a quantity value of the particle. If the particle doesn't hold value of quantity with given
    /// ID, returns unitialized (empty) Value.
    Value getValue(const QuantityId id) const;

    /// Retrieves a quantity derivative of the particle. If the particle doesn't hold derivative of quantity
    /// with given ID, returns unitialized (empty) Value.
    Value getDt(const QuantityId id) const;

    /// Retrieves a quantity 2nd derivative of the particle. If the particle doesn't hold 2nd derivative of
    /// quantity with given ID, returns unitialized (empty) Value.
    Value getD2t(const QuantityId id) const;

    /// Stored info about a quantity. Note that not all values have to be initialized; if they are, however,
    /// all values have the same type.
    struct QuantityData {

        /// Quantity ID
        QuantityId id;

        /// Value type of the quantity
        ValueId type;

        /// Quantity value
        Value value;

        /// First derivative of the quantity
        Value dt;

        /// Second derivative of the quantity
        Value d2t;
    };

    /// Iterator used to enumerate all stored quantities.
    class ValueIterator {
    private:
        using Iterator = std::map<QuantityId, InternalQuantityData>::const_iterator;

        Iterator iter;

    public:
        /// Constructs the iterator from internal type; use \ref Particle::begin and \ref Particle::end to
        /// obtain iterators.
        ValueIterator(const Iterator iterator);

        /// Advances the iterator to next quantity.
        ValueIterator& operator++();

        /// Returns all data associated with currently referenced quantity.
        QuantityData operator*() const;

        /// Inequality operator
        bool operator!=(const ValueIterator& other) const;
    };

    /// Returns iterator pointing to the first quantity.
    ValueIterator begin() const;

    /// Returns iterator pointing to the one-past-last quantity.
    ValueIterator end() const;
};

NAMESPACE_SPH_END
