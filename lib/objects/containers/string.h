#pragma once

#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

class String {
private:
    Array<char> data;

public:
    String()
        : data({ '\0' }) {}

    INLINE char& operator[](const Size idx) {
        return data[idx];
    }

    INLINE char operator[](const Size idx) const {
        return data[idx];
    }

    template <typename T0, typename... TArgs>
    static String format(T0&& t0, TArgs&&... rest) {
        String s;
        s << t0;
        return s + format(std::forward<TArgs>(rest)...);
    }
};

NAMESPACE_SPH_END
