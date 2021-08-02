#pragma once

#include "objects/wrappers/SharedPtr.h"

/// \file SharedToken.h
/// \brief Helper class allowing to own a SharedPtr without knowing its type.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

NAMESPACE_SPH_BEGIN

namespace Detail {

class TokenBlock : public ControlBlockHolder {
public:
    TokenBlock() = default;

    INLINE virtual void* getPtr() override {
        return nullptr;
    }

    virtual void deletePtr() override {
        // no-op
    }
};

} // namespace Detail

class SharedToken {
    friend class WeakToken;

private:
    Detail::ControlBlockHolder* block;

public:
    SharedToken()
        : block(alignedNew<Detail::TokenBlock>()) {}

    SharedToken(std::nullptr_t)
        : block(nullptr) {}

    SharedToken(const SharedToken& other)
        : block(nullptr) {
        *this = other;
    }

    template <typename T>
    SharedToken(const SharedPtr<T>& ptr) {
        if (ptr.block) {
            block = ptr.block;
            block->increaseUseCnt();
            block->increaseWeakCnt();
        } else {
            block = nullptr;
        }
    }

    SharedToken& operator=(const SharedToken& other) {
        this->reset();
        if (other.block) {
            block = other.block;
            block->increaseUseCnt();
            block->increaseWeakCnt();
        } else {
            block = nullptr;
        }
        return *this;
    }

    ~SharedToken() {
        this->reset();
    }

    void reset() {
        if (block) {
            block->decreaseUseCnt();
            block->decreaseWeakCnt();
            block = nullptr;
        }
    }


    INLINE explicit operator bool() const {
        return block != nullptr;
    }

    INLINE bool operator!() const {
        return !this->operator bool();
    }
};


class WeakToken {
private:
    Detail::ControlBlockHolder* block;

public:
    WeakToken()
        : block(nullptr) {}

    WeakToken(std::nullptr_t)
        : block(nullptr) {}

    WeakToken(const WeakToken& other)
        : block(other.block) {
        if (block) {
            block->increaseWeakCnt();
        }
    }

    WeakToken(const SharedToken& other)
        : block(other.block) {
        if (block) {
            block->increaseWeakCnt();
        }
    }

    template <typename T>
    WeakToken(const SharedPtr<T>& ptr)
        : block(ptr.block) {
        if (block) {
            block->increaseWeakCnt();
        }
    }

    WeakToken& operator=(const WeakToken& other) {
        block = other.block;
        if (block) {
            block->increaseWeakCnt();
        }
        return *this;
    }

    SharedToken lock() const {
        if (block && block->increaseUseCntIfNonzero()) {
            SharedToken token;
            token.block = block;
            block->increaseWeakCnt();
            return token;
        } else {
            return nullptr;
        }
    }
};

NAMESPACE_SPH_END
