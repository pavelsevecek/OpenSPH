#pragma once

#include "objects/containers/FlatMap.h"
#include "objects/utility/Dynamic.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

class Storage;
enum class QuantityId;

/// \brief Object holding information about single particle
///
/// Particle can store all or some quantities of given particle. Unlike in \ref Storage, the \ref Particle
/// does not hold information about orders of the stored quantities, and it is possible to store derivatives
/// of quantities without having quantity values stored as well.
class Particle {
    friend struct ParticleVisitor;

private:
    Size idx;

    struct InternalQuantityData {
        Dynamic value;
        Dynamic dt;
        Dynamic d2t;
    };

    FlatMap<QuantityId, InternalQuantityData> quantities;

    FlatMap<BodySettingsId, Dynamic> material;

public:
    /// \brief Default constructor, defined only for convenient usage in containers, etc.
    ///
    /// Particle has to be initialized using copy/move operator.
    Particle() = default;

    /// \brief Constructs object given its index in parent storage, containing no particle data.
    ///
    /// Quantity data can be added using functions \ref addValue, \ref addDt and \ref addD2t.
    explicit Particle(const Size idx)
        : idx(idx) {}

    /// \brief Constructs the object from storage, storing values of all quantities for given particle
    ///
    /// \param storage Storage containing the particle
    /// \param idx Index of the particle
    Particle(const Storage& storage, const Size idx);

    /// \brief Constructs a particle with information about a single quantity value.
    ///
    /// Other quantity data can be added using functions \ref addValue, \ref addDt and \ref addD2t.
    /// \param id Quantity ID of the quantity value
    /// \param value Quantity value. Cannot be used to add derivatives.
    /// \param idx Index of particle; although this constructor is not necessarily bound to a particle
    ///            storage, this represents index to storage associated with the particle.
    Particle(const QuantityId id, const Dynamic& value, const Size idx);

    Particle(const Particle& other);

    Particle(Particle&& other);

    Particle& operator=(const Particle& other);

    Particle& operator=(Particle&& other);

    /// \brief Adds another quantity value or updates the value of quantity previously stored.
    ///
    /// \param id Quantity ID of the quantity value
    /// \param value New value of given quantity
    /// \return Reference to itself, allowing to queue functions
    Particle& addValue(const QuantityId id, const Dynamic& value);

    /// \brief Adds another quantity derivative or updates the derivative of quantity previously stored.
    ///
    /// \param id Quantity ID of the quantity value
    /// \param value New derivative of given quantity
    /// \return Reference to itself, allowing to queue functions
    Particle& addDt(const QuantityId id, const Dynamic& value);

    /// \brief Adds another quantity 2nd derivative or updates the 2nd derivative of quantity previously
    /// stored.
    ///
    /// \param id Quantity ID of the quantity value
    /// \param value New 2nd derivative of given quantity
    /// \return Reference to itself, allowing to queue functions
    Particle& addD2t(const QuantityId id, const Dynamic& value);

    /// \brief Adds another material parameter or updates the one stored previously.
    ///
    /// \param id ID of the material parameter
    /// \param value Value of the material parameter
    Particle& addParameter(const BodySettingsId id, const Dynamic& value);

    /// \brief Returns the index of particle in the parent storage.
    INLINE Size getIndex() const {
        return idx;
    }

    /// \brief Retrieves a quantity value of the particle.
    ///
    /// If the particle doesn't hold value of quantity with given ID, returns unitialized (empty) value.
    Dynamic getValue(const QuantityId id) const;

    /// \brief Retrieves a quantity derivative of the particle.
    ///
    /// If the particle doesn't hold derivative of quantity with given ID, returns unitialized (empty) value.
    Dynamic getDt(const QuantityId id) const;

    /// \brief Retrieves a quantity 2nd derivative of the particle.
    ///
    /// If the particle doesn't hold 2nd derivative of quantity with given ID, returns unitialized (empty)
    /// value.
    Dynamic getD2t(const QuantityId id) const;

    /// \brief Retrieves a material parameter of the particle.
    ///
    /// If the particle doesn't hold the parameter with given ID, returns unitialized (empty) value.
    Dynamic getParameter(const BodySettingsId id) const;

    /// \brief Stored info about a quantity.
    ///
    /// Note that not all values have to be initialized; if they are, however, all values have the same type.
    struct QuantityData {

        /// Quantity ID
        QuantityId id;

        /// Value type of the quantity
        DynamicId type;

        /// Quantity value
        Dynamic value;

        /// First derivative of the quantity
        Dynamic dt;

        /// Second derivative of the quantity
        Dynamic d2t;
    };

    /// \brief Iterator used to enumerate all stored quantities.
    class QuantityIterator {
    private:
        using ActIterator = Iterator<const FlatMap<QuantityId, InternalQuantityData>::Element>;

        ActIterator iter;

    public:
        /// \brief Constructs the iterator from internal type.
        ///
        /// Cannot be constructed directly, use \ref Particle::getQuantities instead.
        QuantityIterator(const ActIterator iterator, Badge<Particle>);

        /// Advances the iterator to next quantity.
        QuantityIterator& operator++();

        /// Returns all data associated with currently referenced quantity.
        QuantityData operator*() const;

        /// Inequality operator
        bool operator!=(const QuantityIterator& other) const;
    };

    /// \brief Helper for enumerating all stored quantities.
    class QuantitySequence {
    private:
        QuantityIterator first;
        QuantityIterator last;

    public:
        /// \brief Creates a sequence of all quantities of given particle.
        QuantitySequence(const Particle& particle);

        /// \brief Returns iterator pointing to the first quantity.
        QuantityIterator begin() const;

        /// \brief Returns iterator pointing to the one-past-last quantity.
        QuantityIterator end() const;
    };

    /// \brief Returns a range for enumerating all stored quantities.
    QuantitySequence getQuantities() const;

    /// \brief Stored info about a material parameter.
    struct ParamData {

        /// Material parameter ID
        BodySettingsId id;

        /// Parameter value
        Dynamic value;
    };

    class ParamIterator {
    private:
        using ActIterator = Iterator<const FlatMap<BodySettingsId, Dynamic>::Element>;

        ActIterator iter;

    public:
        /// \brief Constructs the iterator from internal type.
        ///
        /// Cannot be constructed directly, use \ref Particle::getParameters instead.
        ParamIterator(const ActIterator iterator, Badge<Particle>);

        /// Advances the iterator to next quantity.
        ParamIterator& operator++();

        /// Returns all data associated with currently referenced parameter.
        ParamData operator*() const;

        /// Inequality operator
        bool operator!=(const ParamIterator& other) const;
    };

    /// \brief Helper for enumerating all stored parameters.
    class ParamSequence {
    private:
        ParamIterator first;
        ParamIterator last;

    public:
        /// \brief Creates a sequence of all material parameters of given particle.
        ParamSequence(const Particle& particle);

        /// \brief Returns iterator pointing to the first parameter.
        ParamIterator begin() const;

        /// \brief Returns iterator pointing to the one-past-last parameter.
        ParamIterator end() const;
    };

    /// \brief Returns a range for enumerating all stored parameters.
    ParamSequence getParameters() const;
};

NAMESPACE_SPH_END
