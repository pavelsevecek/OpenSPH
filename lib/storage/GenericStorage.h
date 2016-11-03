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
private:
    using ScalarQuantity = Array<Float>;
    using VectorQuantity = Array<Vector>;

    Array<ScalarQuantity> scalars;
    Array<VectorQuantity> vectors;

    Tuple<Iterables<IterableType::FIRST_ORDER>, Iterables<IterableType::SECOND_ORDER>> iterables;

public:
    GenericStorage() = default;

    template <QuantityType Type>
    INLINE decltype(auto) view(const int idx);

    template <typename TViewer>
    std::unique_ptr<TViewer> makeViewer() {
        scalars.resize(TViewer::getQuantityCnt(QuantityType::SCALAR));
        vectors.resize(TViewer::getQuantityCnt(QuantityType::VECTOR));
        std::unique_ptr<TViewer> viewer = std::make_unique<TViewer>(*this);
        get<int(IterableType::FIRST_ORDER)>(iterables) =
            viewer->template getIterables<IterableType::FIRST_ORDER>();
        get<int(IterableType::SECOND_ORDER)>(iterables) =
            viewer->template getIterables<IterableType::SECOND_ORDER>();
        return viewer;
    }

    template<IterableType Type>
    Iterables<Type>& getIterables() {
        return get<int(Type)>(iterables);
    }


    virtual void merge(const GenericStorage& other) {
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
