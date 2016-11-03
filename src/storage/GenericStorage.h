#pragma once

#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

enum class QuantityType {
    SCALAR,           ///< Scalar quantity (array of floats)
    VECTOR,           ///< Vector quantity (array of vectors, or 4 floats)
    TENSOR,           ///< Symmetric tensor quantity (array of symmetric matrices, or 6 floats)
    TRACELESS_TENSOR, ///< Traceless symmetric tensor quantity (array of symmetric traceless matrices, or 5
                      ///  floats)
};


/// Base object for storing scalar, vector and tensor quantities of SPH particles.
class GenericStorage : public Noncopyable {
private:
    using ScalarQuantity = Array<Float>;
    using VectorQuantity = Array<Vector>;

    Array<ScalarQuantity> scalars;
    Array<VectorQuantity> vectors;

public:
    GenericStorage() = default;

    template <QuantityType Type>
    INLINE decltype(auto) view(const int idx);

    template <typename TViewer>
    std::unique_ptr<TViewer> makeViewer() {
        scalars.resize(TViewer::getQuantityCnt(QuantityType::SCALAR));
        vectors.resize(TViewer::getQuantityCnt(QuantityType::VECTOR));
        return std::make_unique<TViewer>(*this);
    }


    virtual void merge(const GenericStorage& other) {
        // must contain the same quantities
        ASSERT(scalars.size() == other.scalars.size());
        ASSERT(vectors.size() == other.vectors.size());
        for (int i=0; i<scalars.size(); ++i) {
            scalars[i].pushAll(other.scalars[i]);
        }
        for (int i=0; i<vectors.size(); ++i) {
            vectors[i].pushAll(other.vectors[i]);
        }
    }

    /// Iterate through all arrays in the storage
    /*template <StorageIterableType Type, typename TFunctor>
    void iterate(TFunctor&& functor) {
        StorageIterator<Type, BasicStorage>::iterate(*this, std::forward<TFunctor>(functor));
    }

    /// Iterate through all arrays in multiple storages at once.
    template <StorageIterableType Type, typename TFunctor>
    static void iteratePair(TFunctor&& functor, GenericStorage& st1, GenericStorage& st2) {
        StorageIterator<Type, BasicStorage>::iteratePair(st1, st2, std::forward<TFunctor>(functor));
    }*/
};

template <>
INLINE decltype(auto) GenericStorage::view<QuantityType::SCALAR>(const int idx) {
    return scalars[idx];
}

template <>
INLINE decltype(auto) GenericStorage::view<QuantityType::VECTOR>(const int idx) {
    return vectors[idx];
}

/// \todo tensor



/*
template <>
struct StorageIterator<IterableType::SECOND_ORDER, BasicStorage> {
    template <typename TFunctor>
    static void iterate(BasicStorage& st, TFunctor&& functor) {
        functor(st.rs, st.vs, st.dvs);
    }

    template <typename TFunctor>
    static void iteratePair(BasicStorage& st1, BasicStorage& st2, TFunctor&& functor) {
        constexpr StorageIterableType type = StorageIterableType::SECOND_ORDER;
        functor(makeWrapper<type>(st1.rs, st1.vs, st1.dvs), makeWrapper<type>(st2.rs, st2.vs, st2.dvs));
    }
};

template <>
struct StorageIterator<StorageIterableType::FIRST_ORDER, BasicStorage> {
    template <typename TFunctor>
    static void iterate(BasicStorage& storage, TFunctor&& functor) {
        functor(storage.rhos, storage.drhos);
        functor(storage.us, storage.dus);
    }

    template <typename TFunctor>
    static void iteratePair(BasicStorage& st1, BasicStorage& st2, TFunctor&& functor) {
        constexpr StorageIterableType type = StorageIterableType::FIRST_ORDER;
        functor(makeWrapper<type>(st1.rhos, st1.drhos), makeWrapper<type>(st2.rhos, st2.drhos));
        functor(makeWrapper<type>(st1.us, st1.dus), makeWrapper<type>(st2.us, st2.dus));
    }
};

template <>
struct StorageIterator<StorageIterableType::ALL, BasicStorage> {
    template <typename TFunctor>
    static void iterate(BasicStorage& storage, TFunctor&& functor) {
        functor(storage.rs);
        functor(storage.vs);
        functor(storage.dvs);
        functor(storage.rhos);
        functor(storage.drhos);
        functor(storage.ps);
        functor(storage.us);
        functor(storage.dus);
        functor(storage.ms);
    }

    template <typename TFunctor>
    static void iteratePair(BasicStorage& st1, BasicStorage& st2, TFunctor&& functor) {
        functor(st1.rs, st2.rs);
        functor(st1.vs, st2.vs);
        functor(st1.dvs, st2.dvs);
        functor(st1.rhos, st2.rhos);
        functor(st1.drhos, st2.drhos);
        functor(st1.ps, st2.ps);
        functor(st1.us, st2.us);
        functor(st1.dus, st2.dus);
        functor(st1.ms, st2.ms);
    }
};
*/

NAMESPACE_SPH_END
