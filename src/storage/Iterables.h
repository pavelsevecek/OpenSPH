#pragma once

#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN


enum class IterableType {
    CONSTANT, ///< No need to solve evolutionary equation for the quantity; it's either constant (mass, ...)
              /// or computed from other quantities (pressure, ...)
    FIRST_ORDER,  ///< Quantity is evolved in time using its first derivatives
    SECOND_ORDER, ///< Quantity is evolved in time using its first and second derivatives derivatives
    DERIVATIVE,   ///< This quantity is a derivative for another quantity
};


/// Auxiliary structure holding together references to quantities related by temporal derivatives.
template <typename TValue, IterableType Type>
struct IterableWrapper;

template <typename TValue>
struct IterableWrapper<TValue, IterableType::FIRST_ORDER> {
    Array<TValue>& f;
    Array<TValue>& df;
};

template <typename TValue>
struct IterableWrapper<TValue, IterableType::SECOND_ORDER> {
    Array<TValue>& f;
    Array<TValue>& df;
    Array<TValue>& d2f;
};

template<IterableType Type>
struct Iterables {
    Array<IterableWrapper<Float, Type>> scalars;
    Array<IterableWrapper<Vector, Type>> vectors;
};

NAMESPACE_SPH_END
