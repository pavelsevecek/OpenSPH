#pragma once

#include "geometry/Vector.h"
#include "objects/containers/Tuple.h"
#include "storage/Storage.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

namespace Detail {

    template <typename T>
    using void_t = void;

    template <typename T, typename TEnabler = void>
    struct HasAccumulateImpl {
        static constexpr bool value = false;
    };
    template <typename T>
    struct HasAccumulateImpl<T,
        void_t<decltype(std::declval<T>().accumulate(std::declval<int>(),
            std::declval<int>(),
            std::declval<const Vector&>()))>> {
        static constexpr bool value = true;
    };

    template <typename T, typename TEnabler = void>
    struct HasUpdateImpl {
        static constexpr bool value = false;
    };
    template <typename T>
    struct HasUpdateImpl<T, void_t<decltype(std::declval<T>().update(std::declval<Storage&>()))>> {
        static constexpr bool value = true;
    };

    template <typename T, typename TEnabler = void>
    struct HasIntegrateImpl {
        static constexpr bool value = false;
    };
    template <typename T>
    struct HasIntegrateImpl<T, void_t<decltype(std::declval<T>().integrate(std::declval<Storage&>()))>> {
        static constexpr bool value = true;
    };

    template <typename T, typename TEnabler = void>
    struct HasInitializeImpl {
        static constexpr bool value = false;
    };
    template <typename T>
    struct HasInitializeImpl<T,
        void_t<decltype(
            std::declval<T>().initialize(std::declval<Storage&>(), std::declval<const BodySettings&>()))>> {
        static constexpr bool value = true;
    };
}

template <typename T>
struct HasAccumulate {
    static constexpr bool value = Detail::HasAccumulateImpl<T>::value;
};
template <typename T>
struct HasUpdate {
    static constexpr bool value = Detail::HasUpdateImpl<T>::value;
};
template <typename T>
struct HasIntegrate {
    static constexpr bool value = Detail::HasIntegrateImpl<T>::value;
};
template <typename T>
struct HasInitialize {
    static constexpr bool value = Detail::HasInitializeImpl<T>::value;
};


/// Basic class for all components of SPH solver, capable of iterating over its child modules. Modules does
/// not own the children, they are only stored as references. Every class derived from module can implement
/// following:
/// i) update(storage) - Called before solver computes derivatives. Used to update all pointers (arrayviews)
///    to quantities and to compute all values needed by the solver before the integration (pressure, for
///    example).
/// ii) accumulate(i, j, grad) - Called for every interacting pair of particles.
/// iii) integrate(storage) - Called after solver computes derivatives. Must compute derivatives of all
///      quantities needed by the module, using accumulated quantities.
/// iv) initialize(storage, bodySettings) - Called when setting initial conditions for given
///     storage.
///
/// If module contains child modules, it can update / accumulate ... children modules using updateModules /
/// accumulateModules ... methods.
template <typename... TArgs>
class Module : public Noncopyable {
private:
    Tuple<TArgs&...> children;

public:
    Module(TArgs&... modules)
        : children(modules...) {}

    INLINE void updateModules(Storage& storage) {
        forEachIf<HasUpdate>(children, [&storage](auto& module) { module.update(storage); });
    }

    INLINE void accumulateModules(const int i, const int j, const Vector& grad) {
        forEachIf<HasAccumulate>(
            children, [i, j, &grad, this](auto& module) { module.accumulate(i, j, grad); });
    }

    INLINE void integrateModules(Storage& storage) {
        forEachIf<HasIntegrate>(children, [&storage](auto& module) { module.integrate(storage); });
    }

    INLINE void initializeModules(Storage& storage, const BodySettings& settings) {
        forEachIf<HasInitialize>(
            children, [&storage, &settings](auto& module) { module.initialize(storage, settings); });
    }
};

NAMESPACE_SPH_END
