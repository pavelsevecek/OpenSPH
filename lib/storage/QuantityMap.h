#pragma once

/// Storage of all quantities of SPH particles.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "storage/Quantity.h"

NAMESPACE_SPH_BEGIN

/// Unique ID of basic quantities of SPH particles
/// Order MUST match order in QuantityMap
namespace QuantityKey {
    enum BasicKey {
        R   = 0, ///< Positions (velocities, accelerations) of particles
        M   = 1, ///< Paricles masses
        P   = 2, ///< Pressure
        RHO = 3, ///< Density
        U   = 4, ///< Specific internal energy
        CS  = 5, ///< Sound speed
    };
};

struct QuantityDescriptor {
    const int key;
    const ValueEnum valueEnum;
    const TemporalEnum temporalEnum;
};


/// Defines type of the value and number of derivatives for given key.
/// Can be extended by partial specialization.
template <int TKey>
struct QuantityMap {
    // clang-format off
    static constexpr QuantityDescriptor quantityDescriptors[] =
        { { QuantityKey::R,     ValueEnum::VECTOR, TemporalEnum::SECOND_ORDER },
          { QuantityKey::M,     ValueEnum::SCALAR, TemporalEnum::CONST },
          { QuantityKey::P,     ValueEnum::SCALAR, TemporalEnum::CONST },
          { QuantityKey::RHO,   ValueEnum::SCALAR, TemporalEnum::FIRST_ORDER },
          { QuantityKey::U,     ValueEnum::SCALAR, TemporalEnum::FIRST_ORDER },
          { QuantityKey::CS,    ValueEnum::SCALAR, TemporalEnum::CONST } };
    // clang-format on

    static constexpr QuantityDescriptor descriptor = quantityDescriptors[int(TKey)];
    using Type                                     = typename GetTypeFromValue<descriptor.valueEnum>::Type;
    static constexpr TemporalEnum temporalEnum     = descriptor.temporalEnum;
};

/// alias
template <int TKey>
using QuantityType = typename QuantityMap<TKey>::Type;


/*
enum class DamageKey { D = 5 };

template <DamageKey Key>
struct QuantityMap<DamageKey, Key> {
    static constexpr QuantityDescriptor<DamageKey> quantityDescriptors[] = {
        { DamageKey::D, ValueEnum::VECTOR, TemporalEnum::FIRST_ORDER }
    };

    static constexpr QuantityDescriptor<DamageKey> descriptor = quantityDescriptors[int(Key)];
    using Type                                 = typename GetTypeFromValue<descriptor.valueEnum>::Type;
    static constexpr TemporalEnum temporalEnum = descriptor.temporalEnum;
};*/


/// Convenient way to access quantities by their name. Does not actually store anyting, requires constructed
/// GenericStorage that owns the arrays.
class BasicView : public Noncopyable {
public:
    /// The number of particles


    /// Constructs by getting references from storage.
    /*   BasicView(GenericStorage& storage)
           : rs(storage.view<QuantityType::VECTOR>(0))
           , vs(storage.view<QuantityType::VECTOR>(1))
           , dvs(storage.view<QuantityType::VECTOR>(2))
           , ms(storage.view<QuantityType::SCALAR>(0))
           , rhos(storage.view<QuantityType::SCALAR>(1))
           , drhos(storage.view<QuantityType::SCALAR>(2))
           , ps(storage.view<QuantityType::SCALAR>(3))
           , us(storage.view<QuantityType::SCALAR>(4))
           , dus(storage.view<QuantityType::SCALAR>(5))
           , storage(storage) {}

       template <IterableType Type>
       INLINE Iterables<Type> getIterables() const;

       static int getQuantityCnt(const QuantityType type) {
           switch (type) {
           case QuantityType::SCALAR:
               return 6;
           case QuantityType::VECTOR:
               return 3;
           default:
               ASSERT(false);
           }
       }

       void init() {
           dvs.fill(Vector(0._f));
           dus.fill(0._f);
           drhos.fill(0._f);
       }

       /// Fill the storage by particles.
       /// \param n Expected number of particles (note that final number of particles can be different, based
       on
       /// selected particle distribution).
       /// \param domain Spatial domain filled with particles.
       /// \param distribution Algorithm for distribution particles in space.
       /// \param settings Settings used to get default values for quantities of SPH particles.
       void create(const int n,
                   const Abstract::Domain* domain,
                   const Abstract::Distribution* distribution,
                   const Settings<BodySettingsIds>& settings);

       /// Moves all particles in given direction. Does not change velocities or other quantities
       void move(const Vector& offset) {
           for (Vector& r : rs) {
               r += offset;
           }
       }

       void rotate(const Vector& UNUSED(center), const Vector& UNUSED(axis)) {
           /// \todo
       }

       /// Adds velocity to all particles.
       void addVelocity(const Vector& velocity) {
           for (Vector& v : vs) {
               v += velocity;
           }
       }


       /// Adds angular velocity around given center and axis of rotation
       void addAngularVelocity(const Vector& UNUSED(center), const Vector& UNUSED(axis)) {
           /// \todo
       }


       /// \todo generalize using some output routine
       void saveToFile(const std::string& path) {
           std::ofstream ofs(path);
           for (int i = 0; i < N; ++i) {
               ofs << i << " " << rs[i] << " " << vs[i] << " " << ms[i] << " " << rhos[i] << " " << ps[i]
                   << std::endl;
           }
           ofs.close();
       }*/
};

/*template <>
INLINE Iterables<IterableType::ALL> BasicView::getIterables<IterableType::ALL>() const {
    return Iterables<IterableType::ALL>{ storage.scalars, storage.vectors };
}*/

/*template <>
INLINE Iterables<IterableType::SECOND_ORDER> BasicView::getIterables<IterableType::SECOND_ORDER>() const {
    return Iterables<IterableType::SECOND_ORDER>{ {}, { { rs, vs, dvs } } };
}

template <>
INLINE Iterables<IterableType::FIRST_ORDER> BasicView::getIterables<IterableType::FIRST_ORDER>() const {
    return Iterables<IterableType::FIRST_ORDER>{ { { rhos, drhos }, { us, dus } }, {} };
}*/

NAMESPACE_SPH_END
