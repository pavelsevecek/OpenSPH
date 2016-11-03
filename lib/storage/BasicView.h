#pragma once

/// Storage of all quantities of SPH particles.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "physics/Eos.h"
#include "storage/GenericStorage.h"
#include "storage/Iterables.h"
#include "system/Factory.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN


/// Convenient way to access quantities by their name. Does not actually store anyting, requires constructed
/// GenericStorage that owns the arrays.
class BasicView : public Noncopyable {
public:
    /// The number of particles
    int N;

    /// \todo separate quantities into "evolved by timestepping" and "others" (others can still be changed
    /// over time, but not by computing its derivatives from the model, like pressure)

    /// Coordinates of particles, fourth component is smoothing length.
    Array<Vector>& rs;

    /// Velocities of particles, fourth component if velocity divergence
    Array<Vector>& vs;

    /// Acceleration
    Array<Vector>& dvs;

    /// Mass of SPH particles
    Array<Float>& ms;

    /// Density
    Array<Float>& rhos;

    /// Density derivative
    Array<Float>& drhos;

    /// Pressure
    Array<Float>& ps;

    /// Specific internal energy (=energy per unit mass)
    Array<Float>& us;

    /// Derivative of internal energy;
    Array<Float>& dus;

    GenericStorage& storage;

    /// Constructs by getting references from storage.
    BasicView(GenericStorage& storage)
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
    /// \param n Expected number of particles (note that final number of particles can be different, based on
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
    }
};

/*template <>
INLINE Iterables<IterableType::ALL> BasicView::getIterables<IterableType::ALL>() const {
    return Iterables<IterableType::ALL>{ storage.scalars, storage.vectors };
}*/

template <>
INLINE Iterables<IterableType::SECOND_ORDER> BasicView::getIterables<IterableType::SECOND_ORDER>() const {
    return Iterables<IterableType::SECOND_ORDER>{ {}, { { rs, vs, dvs } } };
}

template <>
INLINE Iterables<IterableType::FIRST_ORDER> BasicView::getIterables<IterableType::FIRST_ORDER>() const {
    return Iterables<IterableType::FIRST_ORDER>{ { { rhos, drhos }, { us, dus } }, {} };
}

NAMESPACE_SPH_END
