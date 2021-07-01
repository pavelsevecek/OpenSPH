#pragma once

/// Operators comparing elements of containers with a value
/// \todo move to some more reasonable location

#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

template <typename TContainer>
class PerElementWrapper {
private:
    TContainer&& container;

    using RawType = std::decay_t<TContainer>;

public:
    PerElementWrapper(TContainer&& container)
        : container(std::forward<TContainer>(container)) {}

    template <typename TValue>
    bool operator==(const TValue& value) const {
        const typename RawType::Type t2 = value;
        for (const auto& t1 : container) {
            if (t1 != t2) {
                return false;
            }
        }
        return true;
    }

    template <typename TValue>
    bool operator!=(const TValue& value) const {
        return !(*this == value);
    }

    template <typename TValue>
    bool operator>(const TValue& value) const {
        const typename RawType::Type t2 = value;
        for (const auto& t1 : container) {
            if (t1 <= t2) {
                return false;
            }
        }
        return true;
    }

    template <typename TValue>
    bool operator>=(const TValue& value) const {
        const typename RawType::Type t2 = value;
        for (const auto& t1 : container) {
            if (t1 < t2) {
                return false;
            }
        }
        return true;
    }

    template <typename TValue>
    bool operator<(const TValue& value) const {
        return !(*this >= value);
    }

    template <typename TValue>
    bool operator<=(const TValue& value) const {
        return !(*this > value);
    }

    friend std::ostream& operator<<(std::ostream& stream, const PerElementWrapper& wrapper) {
        stream << wrapper.container;
        return stream;
    }
};

template <typename TContainer>
PerElementWrapper<TContainer> perElement(TContainer&& container) {
    return PerElementWrapper<TContainer>(std::forward<TContainer>(container));
}

NAMESPACE_SPH_END
