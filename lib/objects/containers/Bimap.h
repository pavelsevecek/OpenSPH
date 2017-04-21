#pragma once

/// Bidirectional map
/// \todo This is currently quite inefficient as it has O(N) complexity for most task.
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

/// Map of pairs Left-Right.
template <typename Left, typename Right>
class Bimap {
private:
    struct Value {
        Left left;
        Right right;
    };
    Array<Value> data;

public:
    Bimap() = default;

    Bimap(std::initializer_list<Value> list) {
        for (Value& v : list) {
            data.push(v);
        }
    }

    void push(const Value& value) {
        data.push(value);
    }

    Right* findRight(const Left& left) {
        for (Value& v : data) {
            if (v.left == left) {
                return &right;
            }
        }
        return nullptr;
    }

    Left* findLeft(const Right& right) {
        for (Value& v : data) {
            if (v.right == right) {
                return &left;
            }
        }
        return nullptr;
    }
};

NAMESPACE_SPH_END
