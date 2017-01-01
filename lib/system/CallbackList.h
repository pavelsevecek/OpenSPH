#pragma once

#include "objects/containers/Array.h"
#include "objects/wrappers/NonOwningPtr.h"

NAMESPACE_SPH_BEGIN

template <typename TSignature>
class CallbackList;

template <typename TReturn, typename... TArgs>
class CallbackList<TReturn(TArgs...)>  {
private:
    struct Callback {
        NonOwningPtr<const Observable> parent;
        std::function<TReturn(TArgs...)> func;
    };
    Array<Callback> callbacks;

public:
    CallbackList() = default;

    void add(const Observable& parent, const std::function<TReturn(TArgs...)>& callback) {
        callbacks.push(Callback{ &parent, callback });
    }

    void operator()(TArgs... args) {
        for (Callback& c : callbacks) {
            if (c.parent) {
                c.func(args...);
            }
        }
    }
};

NAMESPACE_SPH_END
