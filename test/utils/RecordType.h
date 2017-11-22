#pragma once

/// \file RecordType.h
/// \brief Helper type tracking the number of copies, moves, etc.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/Object.h"
#include <ostream>

NAMESPACE_SPH_BEGIN

struct RecordType {
    bool wasMoved = false;
    bool wasMoveConstructed = false;
    bool wasCopyConstructed = false;
    bool wasMoveAssigned = false;
    bool wasCopyAssigned = false;
    bool wasDefaultConstructed = false;
    bool wasValueConstructed = false;
    bool wasSwapped = false;

    static int constructedNum;
    static int destructedNum;

    static void resetStats() {
        constructedNum = 0;
        destructedNum = 0;
    }

    static int existingNum() {
        return constructedNum - destructedNum;
    }

    int value = -1;

    RecordType() {
        wasDefaultConstructed = true;
        constructedNum++;
    }

    ~RecordType() {
        destructedNum++;
    }

    RecordType(const int value)
        : value(value) {
        wasValueConstructed = true;
        constructedNum++;
    }

    RecordType(const RecordType& other) {
        wasCopyConstructed = true;
        value = other.value;
        constructedNum++;
    }

    RecordType(RecordType&& other) {
        wasMoveConstructed = true;
        other.wasMoved = true;
        value = other.value;
        constructedNum++;
    }

    RecordType& operator=(const RecordType& other) {
        wasCopyAssigned = true;
        value = other.value;
        return *this;
    }

    RecordType& operator=(RecordType&& other) {
        wasMoveAssigned = true;
        other.wasMoved = true;
        value = other.value;
        return *this;
    }

    bool operator==(const RecordType& other) const {
        return value == other.value;
    }

    bool operator!=(const RecordType& other) const {
        return value != other.value;
    }

    friend std::ostream& operator<<(std::ostream& ofs, const RecordType& r) {
        ofs << r.value;
        return ofs;
    }
};

template <typename T>
struct IsRecordType {
    static constexpr bool value = false;
};
template <>
struct IsRecordType<RecordType> {
    static constexpr bool value = true;
};

NAMESPACE_SPH_END

namespace std {
    template <>
    inline void swap(Sph::RecordType& r1, Sph::RecordType& r2) {
        swap(r1.value, r2.value);
        r1.wasSwapped = true;
        r2.wasSwapped = true;
    }
} // namespace std
