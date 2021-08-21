#pragma once

/// \file Serializer.h
/// \brief Data serialization and deserialization
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "io/Path.h"
#include "objects/Exceptions.h"
#include "objects/containers/Array.h"
#include "objects/geometry/Tensor.h"
#include "objects/utility/Streams.h"
#include "objects/wrappers/Optional.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

namespace Detail {
/// \brief Type trait for serialization/deserialization of types.
///
/// Every type used in serialization must explicitly specialize the class. Only std::string has dedicated
/// member functions in (de)serializer classes and does not have to be specialized.
template <bool Precise, typename T, typename TEnabler = void>
struct SerializedType;

template <bool Precise, typename T>
struct SerializedType<Precise, T, std::enable_if_t<std::is_integral<T>::value || std::is_enum<T>::value>> {
    // convert all intergral types and enums to int64_t
    using Type = std::conditional_t<Precise, int64_t, int32_t>;
};
template <bool Precise, typename T>
struct SerializedType<Precise, T, std::enable_if_t<std::is_floating_point<T>::value>> {
    // convert floating point types to double
    using Type = std::conditional_t<Precise, double, float>;
};
template <bool Precise, Size N>
struct SerializedType<Precise, char[N]> {
    // keep char[] as is
    using Type = std::add_lvalue_reference_t<const char[N]>;
};

template <bool Precise, typename T>
using Serialized = typename SerializedType<Precise, T>::Type;

} // namespace Detail

/// \brief Object providing serialization of primitives into a stream
template <bool Precise>
class Serializer : public Noncopyable {
private:
    AutoPtr<IBinaryOutputStream> stream;
    Array<char> buffer;

public:
    using View = ArrayView<const char>;

    explicit Serializer(AutoPtr<IBinaryOutputStream>&& stream)
        : stream(std::move(stream)) {}

    template <typename... TArgs>
    View serialize(const TArgs&... args) {
        buffer.clear();
        this->serializeImpl(buffer, args...);
        stream->write(buffer);
        return buffer;
    }

    View write(const bool value) {
        return this->serialize(value);
    }
    View write(const int value) {
        return this->serialize(value);
    }
    View write(const Size value) {
        return this->serialize(value);
    }
    View write(const Float& value) {
        return this->serialize(value);
    }
    View write(const Interval& value) {
        return this->serialize(value.lower(), value.upper());
    }
    View write(const Vector& value) {
        return this->serialize(value[X], value[Y], value[Z], value[H]);
    }
    View write(const SymmetricTensor& t) {
        return this->serialize(t(0, 0), t(1, 1), t(2, 2), t(0, 1), t(0, 2), t(1, 2));
    }
    View write(const TracelessTensor& t) {
        return this->serialize(t(0, 0), t(1, 1), t(0, 1), t(0, 2), t(1, 2));
    }
    View write(const Tensor& t) {
        return this->serialize(
            t(0, 0), t(0, 1), t(0, 2), t(1, 0), t(1, 1), t(1, 2), t(2, 0), t(2, 1), t(2, 2));
    }
    View write(const EnumWrapper& e) {
        // Type hash can be different between invocations, so we cannot serialize it. Write 0 (for backward
        // compatibility)
        return this->serialize(e.value, 0);
    }
    template <std::size_t N>
    View write(char (&data)[N]) {
        return this->serialize(data);
    }
    View write(const String& s) {
        return this->serialize(s);
    }

    View addPadding(const Size size) {
        buffer.resize(size);
        buffer.fill('\0');
        stream->write(buffer);
        return buffer;
    }

private:
    template <typename T0, typename... TArgs>
    void serializeImpl(Array<char>& bytes, const T0& t0, const TArgs&... args) {
        const Size size = bytes.size();
        using ActType = Detail::Serialized<Precise, T0>;

        bytes.resize(size + sizeof(ActType));
        ActType serializable = static_cast<ActType>(t0);
        const char* c = reinterpret_cast<const char*>(&serializable);
        for (Size i = 0; i < sizeof(ActType); ++i) {
            bytes[size + i] = c[i];
        }
        this->serializeImpl(bytes, args...);
    }

    template <typename... TArgs>
    void serializeImpl(Array<char>& bytes, const String& s, const TArgs&... args) {
        const Size size = bytes.size();
        CharString s8 = s.toUtf8();
        bytes.resize(size + Size(s8.size()) + 1);
        for (Size i = 0; i < s8.size(); ++i) {
            bytes[size + i] = s8[i];
        }
        bytes[size + s8.size()] = '\0';
        SPH_ASSERT(bytes[bytes.size() - 1] == '\0');
        this->serializeImpl(bytes, args...);
    }

    void serializeImpl(Array<char>& UNUSED(bytes)) {}
};

/// \brief Exception thrown by \ref Deserializer on failure
class SerializerException : public Exception {
public:
    using Exception::Exception;
};

/// \brief Object for reading serialized primitives from input stream
template <bool Precise>
class Deserializer : public Noncopyable {
private:
    AutoPtr<IBinaryInputStream> stream;
    Array<char> buffer;

public:
    explicit Deserializer(AutoPtr<IBinaryInputStream>&& stream)
        : stream(std::move(stream)) {}

    /// Deserialize a list of parameters from the binary file.
    /// \return True on success, false if at least one parameter couldn't be read.
    /// \note Strings can be read with fixed length by passing char[], or by reading until first \0 by passing
    ///       std::string parameter.
    /// \throw SerializerException if reading failed
    template <typename... TArgs>
    void deserialize(TArgs&... args) {
        return this->deserializeImpl(args...);
    }

    void read(bool& value) {
        return this->deserialize(value);
    }
    void read(int& value) {
        return this->deserialize(value);
    }
    void read(Size& value) {
        return this->deserialize(value);
    }
    void read(Float& value) {
        return this->deserialize(value);
    }
    void read(Interval& value) {
        Float lower, upper;
        this->deserialize(lower, upper);
        value = Interval(lower, upper);
    }
    void read(Vector& value) {
        this->deserialize(value[X], value[Y], value[Z], value[H]);
    }
    void read(SymmetricTensor& t) {
        this->deserialize(t(0, 0), t(1, 1), t(2, 2), t(0, 1), t(0, 2), t(1, 2));
    }
    void read(TracelessTensor& t) {
        StaticArray<Float, 5> a;
        this->deserialize(a[0], a[1], a[2], a[3], a[4]);
        t = TracelessTensor(a[0], a[1], a[2], a[3], a[4]);
    }
    void read(Tensor& t) {
        this->deserialize(t(0, 0), t(0, 1), t(0, 2), t(1, 0), t(1, 1), t(1, 2), t(2, 0), t(2, 1), t(2, 2));
    }
    void read(EnumWrapper& e) {
        int dummy;
        this->deserialize(e.value, dummy);
    }
    void read(String& s) {
        this->deserialize(s);
    }

    /// Skip a number of bytes in the stream; used to skip unused parameters or padding bytes.
    void skip(const Size size) {
        if (!stream->skip(size)) {
            this->fail("Failed to skip {} bytes in the stream", size);
        }
    }

private:
    template <typename T0, typename... TArgs>
    void deserializeImpl(T0& t0, TArgs&... args) {
        static_assert(!std::is_array<T0>::value, "String must be read as std::string");
        using ActType = Detail::Serialized<Precise, T0>;
        static_assert(sizeof(ActType) > 0, "Invalid size of ActType");
        buffer.resize(sizeof(ActType));
        if (!stream->read(buffer)) {
            /// \todo maybe print the name of the primitive?
            this->fail("Failed to read a primitive of size {}", sizeof(ActType));
        }
        t0 = T0(reinterpret_cast<ActType&>(buffer[0]));
        this->deserializeImpl(args...);
    }

    template <std::size_t N, typename... TArgs>
    void deserializeImpl(char (&ar)[N], TArgs&... args) {
        if (!stream->read(ArrayView<char>(ar, N))) {
            this->fail("Failed to read an array of size {}", N);
        }
        this->deserializeImpl(args...);
    }

    template <typename... TArgs>
    void deserializeImpl(String& s, TArgs&... args) {
        buffer.clear();
        char c;
        while (stream->read(getSingleValueView(c)) && c != '\0') {
            buffer.push(c);
        }
        buffer.push('\0');
        s = String::fromUtf8(&buffer[0]);
        if (!stream->good()) {
            this->fail("Error while deseralizing string from stream, got: {}", s);
        }
        this->deserializeImpl(args...);
    }

    void deserializeImpl() {}

    template <typename... TArgs>
    void fail(const String& error, const TArgs&... args) {
        throw SerializerException(error, args...);
    }
};

NAMESPACE_SPH_END
