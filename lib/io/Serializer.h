#pragma once

/// \file Serializer.h
/// \brief Data serialization and deserialization
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "io/Path.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/Optional.h"
#include <fstream>

NAMESPACE_SPH_BEGIN

namespace Detail {
    /// Type trait for serialization/deserialization of types. Every type used in serialization must
    /// explicitly specialize the class. Only std::string has dedicated member functions in (de)serializer
    /// classes and does not have to be specialized.
    template <typename T, typename TEnabler = void>
    struct SerializedType;

    template <typename T>
    struct SerializedType<T, std::enable_if_t<std::is_integral<T>::value || std::is_enum<T>::value>> {
        // convert all intergral types and enums to int64_t
        using Type = int64_t;
    };
    template <typename T>
    struct SerializedType<T, std::enable_if_t<std::is_floating_point<T>::value>> {
        // convert floating point types to double
        using Type = double;
    };
    template <Size N>
    struct SerializedType<char[N]> {
        // keep char[] as is
        using Type = std::add_lvalue_reference_t<const char[N]>;
    };

    template <typename T>
    using Serialized = typename SerializedType<T>::Type;
}

/// \brief Object providing serialization of primitives into a stream
class Serializer : public Noncopyable {
private:
    std::ofstream ofs;
    Array<char> buffer;

public:
    Serializer(const Path& path)
        : ofs(path.native(), std::ios::binary) {}

    template <typename... TArgs>
    void write(const TArgs&... args) {
        buffer.clear();
        this->serializeImpl(buffer, args...);
        ofs.write(&buffer[0], buffer.size());
    }

    void addPadding(const Size size) {
        buffer.resize(size);
        buffer.fill('\0');
        ofs.write(&buffer[0], size);
    }

    void flush() {
        ofs.flush();
    }

private:
    template <typename T0, typename... TArgs>
    void serializeImpl(Array<char>& bytes, const T0& t0, const TArgs&... args) {
        const Size size = bytes.size();
        using ActType = Detail::Serialized<T0>;
        bytes.resize(size + sizeof(ActType));
        ActType serializable = static_cast<ActType>(t0);
        const char* c = reinterpret_cast<const char*>(&serializable);
        for (Size i = 0; i < sizeof(ActType); ++i) {
            bytes[size + i] = c[i];
        }
        this->serializeImpl(bytes, args...);
    }

    template <typename... TArgs>
    void serializeImpl(Array<char>& bytes, const std::string& s, const TArgs&... args) {
        const Size size = bytes.size();
        bytes.resize(size + s.size() + 1);
        const char* c = s.c_str();
        for (Size i = 0; i < s.size() + 1; ++i) {
            bytes[size + i] = c[i];
        }
        ASSERT(bytes[bytes.size() - 1] == '\0');
        this->serializeImpl(bytes, args...);
    }

    void serializeImpl(Array<char>& UNUSED(bytes)) {}
};

/// \brief Exception thrown by \ref Deserializer on failure
class SerializerException : public std::exception {
private:
    std::string error;
    Size position;

public:
    /// \param error Error message
    /// \param position Position in the stream where the error appeared
    SerializerException(const std::string& error, const Size position)
        : error(error)
        , position(position) {}

    virtual const char* what() const noexcept override {
        return error.c_str();
    }
};

/// \brief Object for reading serialized primitives from input stream
class Deserializer : public Noncopyable {
private:
    std::ifstream ifs;
    Array<char> buffer;

public:
    /// Opens a stream associated with given path and prepares the file for deserialization.
    Deserializer(const Path& path)
        : ifs(path.native(), std::ios::binary) {
        if (!ifs) {
            this->fail("Cannot open file " + path.native() + " for reading.");
        }
    }

    /// Deserialize a list of parameters from the binary file.
    /// \return True on success, false if at least one parameter couldn't be read.
    /// \note Strings can be read with fixed length by passing char[], or by reading until first \0 by passing
    ///       std::string parameter.
    /// \throw SerializerException if reading failed
    template <typename... TArgs>
    void read(TArgs&... args) {
        return this->readImpl(args...);
    }

    /// Skip a number of bytes in the stream; used to skip unused parameters or padding bytes.
    void skip(const Size size) {
        if (!ifs.seekg(size, ifs.cur)) {
            this->fail("Failed to skip " + std::to_string(size) + " bytes in the stream");
        }
    }

private:
    template <typename T0, typename... TArgs>
    void readImpl(T0& t0, TArgs&... args) {
        static_assert(!std::is_array<T0>::value, "String must be read as std::string");
        using ActType = Detail::Serialized<T0>;
        buffer.resize(sizeof(ActType));
        if (!ifs.read(&buffer[0], sizeof(ActType))) {
            /// \todo maybe print the name of the primitive?
            this->fail("Failed to read a primitive of size " + std::to_string(sizeof(ActType)));
        }
        t0 = T0(reinterpret_cast<ActType&>(buffer[0]));
        this->readImpl(args...);
    }

    template <typename... TArgs>
    void readImpl(std::string& s, TArgs&... args) {
        buffer.clear();
        char c;
        while (ifs.read(&c, 1) && c != '\0') {
            buffer.push(c);
        }
        buffer.push('\0');
        s = &buffer[0];
        if (!ifs) {
            this->fail("Error while deseralizing string from stream, got: " + s);
        }
        this->readImpl(args...);
    }

    void readImpl() {}

    void fail(const std::string& error) {
        throw SerializerException(error, ifs.tellg());
    }
};

NAMESPACE_SPH_END