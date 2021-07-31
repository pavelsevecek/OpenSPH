#pragma once

#include "common/Assert.h"

NAMESPACE_SPH_BEGIN

/// \brief Tag type specifying that the input array may contain multiple equal elements.
struct ElementsCommonTag {};

const ElementsCommonTag ELEMENTS_COMMON;

/// \brief Tag type specifying that the input array contains unique elements.
struct ElementsUniqueTag {};

const ElementsUniqueTag ELEMENTS_UNIQUE;

/// \brief Tag type specifying that the input array is already sorted and contains unique elements.
struct ElementsSortedUniqueTag {};

const ElementsSortedUniqueTag ELEMENTS_SORTED_UNIQUE;

NAMESPACE_SPH_END
