#pragma once

/// \file Serializable.h
/// \brief Base class of objects capable of (de)serialization (from) to a byte array
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/containers/Array.h"
#include <typeinfo>

NAMESPACE_SPH_BEGIN

class ISerializable : public Polymorphic {
public:
    /// \brief Serializes the object into a buffer.
    virtual void serialize(Array<uint8_t>& buffer) const = 0;

    /// \brief Creates the object by deserializing the data in a buffer.
    virtual void deserialize(ArrayView<uint8_t> buffer) = 0;

    virtual Size handle() const = 0;
};

/// Base class, can be used for POD types
template <typename Type>
class Serialized : public ISerializable {
private:
    Type value;

    static_assert(std::is_pod<Type>::value,
        "Base class of Serialized wrapper can be only used for POD types, use explicit specialization for "
        "other types");

public:
    Serialized(const Type& value)
        : value(value) {}

    virtual void serialize(Array<uint8_t>& buffer) const override {
        buffer.resize(sizeof(Type));
        memcpy(&buffer[0], &value, sizeof(Type));
    }

    virtual void deserialize(ArrayView<uint8_t> buffer) override {
        ASSERT(buffer.size() >= sizeof(Type));
        memcpy(&value, &buffer[0], sizeof(Type));
    }

    virtual Size handle() const override {
        return typeid(Type).hash_code();
    }
};

template <>
class Serialized<std::string> : public ISerializable {
private:
    std::string value;

public:
    Serialized(const std::string& value)
        : value(value) {}

    virtual void serialize(Array<uint8_t>& buffer) const override {
        buffer.resize(value.size());
        memcpy(&buffer[0], value.data(), value.size());
    }

    virtual void deserialize(ArrayView<uint8_t> buffer) override {
        // assuming null-terminated string
        const char* data = reinterpret_cast<const char*>(&buffer[0]);
        const Size length = strlen(data);
        value.resize(length);
        memcpy(&value[0], &buffer[0], length);
    }

    virtual Size handle() const override {
        return typeid(std::string).hash_code();
    }
};

NAMESPACE_SPH_END
