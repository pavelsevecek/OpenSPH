#pragma once

#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

struct RecordType {
    bool wasMoved = false;
    bool wasMoveConstructed = false;
    bool wasCopyConstructed = false;
    bool wasMoveAssigned = false;
    bool wasCopyAssigned = false;
    bool wasDefaultConstructed = false;
    bool wasValueConstructed = false;

    int value = -1;

    RecordType() { wasDefaultConstructed = true; }

    RecordType(const int value)
        : value(value) {
        wasValueConstructed = true;
    }

    RecordType(const RecordType& other) {
        wasCopyConstructed = true;
        value = other.value;
    }

    RecordType(RecordType&& other) {
        wasMoveConstructed = true;
        other.wasMoved = true;
        value = other.value;
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
};

NAMESPACE_SPH_END
