#pragma once

/// \file Serializable.h
/// \brief Base class of objects capable of (de)serialization (from) to a byte array
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

class Serializable : public Polymorphic {
public:
    /// \brief Serializes the object into a buffer.
    virtual void serialize(Array<uint8_t>& buffer) const = 0;

    /// \brief Creates the object by deserializing the data in a buffer.
    virtual void deserialize(ArrayView<uint8_t> buffer) = 0;

    virtual Size handle() const = 0;

    virtual void registerHandle(const Size handle) = 0;
};

class SerializableTask : public Serializable {
public:
    virtual void operator()() = 0;
};

NAMESPACE_SPH_END
