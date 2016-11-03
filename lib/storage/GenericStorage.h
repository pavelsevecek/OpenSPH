#pragma once

#include "objects/containers/Array.h"
#include "objects/containers/Tuple.h"
#include "storage/Iterables.h"

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
    template <IterableType Type>
    friend struct StorageIterator;

private:
    using ScalarQuantity = Array<Float>;
    using VectorQuantity = Array<Vector>;

    Array<ScalarQuantity> scalars;
    Array<VectorQuantity> vectors;

    using TIterables = Tuple<Iterables<IterableType::FIRST_ORDER>, Iterables<IterableType::SECOND_ORDER>>;
    TIterables iterables;

public:
    GenericStorage() = default;
/*
    GenericStorage(Array<ScalarQuantity>&& otherScalars,
                   Array<VectorQuantity>&& otherVectors) {
                   //const TIterables& otherIterables)
        //: iterables(otherIterables) {
        scalars.reserve(otherScalars.size());
        for (int i = 0; i < otherScalars.size(); ++i) {
            scalars[i].push(otherScalars[i].clone());
        }
        vectors.reserve(otherVectors.size());
        for (int i = 0; i < vectors.size(); ++i) {
            vectors[i].push(otherVectors[i].clone());
        }
    }*/

    /// Constructs storage from given viewer type and creates new iterables. Returns new viewer object with
    /// references initialized to arrays of this storage. Can be used only once.
    template <typename TViewer>
    std::unique_ptr<TViewer> emplace() {
        scalars.resize(TViewer::getQuantityCnt(QuantityType::SCALAR));
        vectors.resize(TViewer::getQuantityCnt(QuantityType::VECTOR));
        std::unique_ptr<TViewer> viewer = std::make_unique<TViewer>(*this);
        get<int(IterableType::FIRST_ORDER)>(iterables) =
            viewer->template getIterables<IterableType::FIRST_ORDER>();
        get<int(IterableType::SECOND_ORDER)>(iterables) =
            viewer->template getIterables<IterableType::SECOND_ORDER>();
        return viewer;
    }

    template <IterableType Type>
    Iterables<Type>& getIterables() {
        return get<int(Type)>(iterables);
    }


    /// Returns idx-th array of given type
    template <QuantityType Type>
    INLINE decltype(auto) view(const int idx);


    void merge(const GenericStorage& other) {
        // must contain the same quantities
        ASSERT(scalars.size() == other.scalars.size());
        ASSERT(vectors.size() == other.vectors.size());
        for (int i = 0; i < scalars.size(); ++i) {
            scalars[i].pushAll(other.scalars[i]);
        }
        for (int i = 0; i < vectors.size(); ++i) {
            vectors[i].pushAll(other.vectors[i]);
        }
    }

    /// \todo clone only time-dependent quantities
    //GenericStorage clone() const { GenericStorage cloned(scalars.clone(), vectors.clone()); }
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


NAMESPACE_SPH_END
