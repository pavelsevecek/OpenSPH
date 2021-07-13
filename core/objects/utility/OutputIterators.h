#pragma once

/// \file OutputIterators.h
/// \brief Helper iterators allowing to save values to containers.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/Assert.h"

NAMESPACE_SPH_BEGIN

/// \brief Helper output iterator that simply ignores the written values.
class NullInserter {
public:
    NullInserter& operator*() {
        return *this;
    }

    NullInserter& operator++() {
        return *this;
    }

    template <typename TValue>
    NullInserter& operator=(TValue&&) {
        return *this;
    }

    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = ptrdiff_t;
    using pointer = void;
    using reference = void;
};

/// \brief Output iterator that inserts the written values to the given iterator using \ref push function.
template <typename TContainer>
class BackInserter {
private:
    TContainer& container;

public:
    explicit BackInserter(TContainer& container)
        : container(container) {}

    BackInserter& operator*() {
        return *this;
    }

    BackInserter& operator++() {
        return *this;
    }

    template <typename TValue>
    BackInserter& operator=(TValue&& value) {
        container.push(std::forward<TValue>(value));
        return *this;
    }

    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = ptrdiff_t;
    using pointer = void;
    using reference = void;
};

template <typename TContainer>
BackInserter<TContainer> backInserter(TContainer& c) {
    return BackInserter<TContainer>(c);
}

NAMESPACE_SPH_END
