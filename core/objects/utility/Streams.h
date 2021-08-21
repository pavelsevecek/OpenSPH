#pragma once

#include "io/Path.h"
#include <fstream>

NAMESPACE_SPH_BEGIN

class IBinaryOutputStream : public Polymorphic {
public:
    virtual bool write(ArrayView<const char> buffer) = 0;
};

class ITextOutputStream : public Polymorphic {
public:
    virtual bool write(const String& text) = 0;

    virtual bool good() const = 0;
};

class IBinaryInputStream : public Polymorphic {
public:
    /// \brief Reads data from the current position into the given buffer.
    virtual bool read(ArrayView<char> buffer) = 0;

    /// \brief Skips given number of bytes in the stream.
    virtual bool skip(const Size cnt) = 0;

    /// \brief Checks if the stream is in a valid state.
    virtual bool good() const = 0;
};

class ITextInputStream : public Polymorphic {
public:
    virtual bool readLine(String& text, const wchar_t delimiter = L'\n') = 0;

    virtual bool readAll(String& text) = 0;

    virtual bool good() const = 0;
};

class FileBinaryOutputStream : public IBinaryOutputStream {
private:
    std::ofstream ofs;

public:
    explicit FileBinaryOutputStream(const Path& path)
        : ofs(path.native(), std::ios::out | std::ios::binary) {}

    virtual bool write(ArrayView<const char> buffer) override {
        ofs.write(&buffer[0], buffer.size());
        return bool(ofs);
    }
};

class FileTextOutputStream : public ITextOutputStream {
private:
    std::wofstream ofs;

public:
    enum class OpenMode {
        WRITE,
        APPEND,
    };

    explicit FileTextOutputStream(const Path& path, const OpenMode mode = OpenMode::WRITE);

    virtual bool write(const String& text) override;

    virtual bool good() const override {
        return bool(ofs);
    }

    std::wofstream& write() {
        return ofs;
    }
};

class FileBinaryInputStream : public IBinaryInputStream {
private:
    std::ifstream ifs;

public:
    explicit FileBinaryInputStream(const Path& path);

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

class FileTextInputStream : public ITextInputStream {
private:
    std::wifstream ifs;

public:
    explicit FileTextInputStream(const Path& path);

    virtual bool readLine(String& text, const wchar_t delimiter = L'\n') override;

    virtual bool readAll(String& text) override;

    virtual bool good() const override {
        return bool(ifs);
    }
};

class StringTextInputStream : public ITextInputStream {
private:
    std::wstringstream ifs;

public:
    explicit StringTextInputStream(const String& string);

    virtual bool readLine(String& text, const wchar_t delimiter = L'\n') override;

    virtual bool readAll(String& text) override;

    virtual bool good() const override {
        return bool(ifs);
    }
};


class StringTextOutputStream : public ITextOutputStream {
private:
    std::wstringstream ofs;

public:
    virtual bool write(const String& text) override;

    virtual bool good() const override {
        return bool(ofs);
    }

    String toString() const {
        return String::fromWstring(ofs.str());
    }
};

NAMESPACE_SPH_END
