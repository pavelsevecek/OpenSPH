#pragma once

/// \file Serializer.h
/// \brief Data serialization and deserialization
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "io/Path.h"
#include "objects/containers/Array.h"
#include "objects/geometry/Tensor.h"
#include "objects/wrappers/Optional.h"
#include "system/Settings.h"
#include <fstream>

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

class IOutputStream : public Polymorphic {
public:
    virtual bool write(ArrayView<const char> buffer) = 0;
};

class FileOutputStream : public IOutputStream {
private:
    std::ofstream ofs;

public:
    explicit FileOutputStream(const Path& path)
        : ofs(path.native(), std::ios::out | std::ios::binary) {}

    virtual bool write(ArrayView<const char> buffer) override {
        ofs.write(&buffer[0], buffer.size());
        return bool(ofs);
    }
};

/// \brief Object providing serialization of primitives into a stream
template <bool Precise>
class Serializer : public Noncopyable {
private:
    AutoPtr<IOutputStream> stream;
    Array<char> buffer;

public:
    using View = ArrayView<const char>;

    explicit Serializer(AutoPtr<IOutputStream>&& stream)
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
    View write(const std::string& s) {
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
    void serializeImpl(Array<char>& bytes, const std::string& s, const TArgs&... args) {
        const Size size = bytes.size();
        bytes.resize(size + Size(s.size()) + 1);
        const char* c = s.c_str();
        for (Size i = 0; i < s.size() + 1; ++i) {
            bytes[size + i] = c[i];
        }
        SPH_ASSERT(bytes[bytes.size() - 1] == '\0');
        this->serializeImpl(bytes, args...);
    }

    void serializeImpl(Array<char>& UNUSED(bytes)) {}
};

/// \brief Exception thrown by \ref Deserializer on failure
class SerializerException : public std::exception {
private:
    std::string error;

public:
    /// \param error Error message
    SerializerException(const std::string& error)
        : error(error) {}

    virtual const char* what() const noexcept override {
        return error.c_str();
    }
};

class IInputStream : public Polymorphic {
public:
    /// \brief Reads data from the current position into the given buffer.
    virtual bool read(ArrayView<char> buffer) = 0;

    /// \brief Skips given number of bytes in the stream.
    virtual bool skip(const Size cnt) = 0;

    /// \brief Checks if the stream is in a valid state.
    virtual bool good() const = 0;
};

class FileInputStream : public IInputStream {
private:
    std::ifstream ifs;

public:
    explicit FileInputStream(const Path& path)
        : ifs(path.native(), std::ios::in | std::ios::binary) {
        if (!ifs) {
            throw SerializerException("Cannot open file " + path.native() + " for reading.");
        }
    }

    virtual bool read(ArrayView<char> buffer) override {
        return bool(ifs.read(&buffer[0], buffer.size()));
    }

    virtual bool skip(const Size cnt) override {
        return bool(ifs.seekg(cnt, ifs.cur));
    }

    virtual bool good() const override {
        return bool(ifs);
    }
};

/// \brief Object for reading serialized primitives from input stream
template <bool Precise>
class Deserializer : public Noncopyable {
private:
    AutoPtr<IInputStream> stream;
    Array<char> buffer;

public:
    explicit Deserializer(AutoPtr<IInputStream>&& stream)
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
    void read(std::string& s) {
        this->deserialize(s);
    }

    /// Skip a number of bytes in the stream; used to skip unused parameters or padding bytes.
    void skip(const Size size) {
        if (!stream->skip(size)) {
            this->fail("Failed to skip " + std::to_string(size) + " bytes in the stream");
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
            this->fail("Failed to read a primitive of size " + std::to_string(sizeof(ActType)));
        }
        t0 = T0(reinterpret_cast<ActType&>(buffer[0]));
        this->deserializeImpl(args...);
    }

    template <std::size_t N, typename... TArgs>
    void deserializeImpl(char (&ar)[N], TArgs&... args) {
        if (!stream->read(ArrayView<char>(ar, N))) {
            this->fail("Failed to read an array of size " + std::to_string(N));
        }
        this->deserializeImpl(args...);
    }

    template <typename... TArgs>
    void deserializeImpl(std::string& s, TArgs&... args) {
        buffer.clear();
        char c;
        while (stream->read(getSingleValueView(c)) && c != '\0') {
            buffer.push(c);
        }
        buffer.push('\0');
        s = &buffer[0];
        if (!stream->good()) {
            this->fail("Error while deseralizing string from stream, got: " + s);
        }
        this->deserializeImpl(args...);
    }

    void deserializeImpl() {}

    void fail(const std::string& error) {
        throw SerializerException(error);
    }
};

NAMESPACE_SPH_END
