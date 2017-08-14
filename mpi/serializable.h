#pragma once

/// \file Serializable.h
/// \brief Base class of objects capable of (de)serialization (from) to a byte array
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017


#include "common/Assert.h"

NAMESPACE_SPH_BEGIN

class Serializable : public Polymorphic {
public:
    /// \brief Serializes the object into a buffer.
    virtual void serialize(Array<uint8_t>& buffer) = 0;

    /// \brief Creates the object by deserializing the data in a buffer.
    ///
    /// \return Moved input buffer.
    virtual ArrayView<uint8_t> deserialize(ArrayView<uint8_t> buffer) = 0;
};

class SerializableTask : public Serializable {
public:
    virtual void operator()() = 0;
};

NAMESPACE_SPH_END
