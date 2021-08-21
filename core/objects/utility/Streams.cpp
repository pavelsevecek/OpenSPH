#include "objects/utility/Streams.h"
#include "objects/Exceptions.h"
#include <codecvt>
#include <locale>

NAMESPACE_SPH_BEGIN

FileTextOutputStream::FileTextOutputStream(const Path& path, const OpenMode mode) {
    ofs.open(path.native(), mode == OpenMode::WRITE ? std::ios::out : std::ios::app);
    std::locale loc(std::locale(), new std::codecvt_utf8<wchar_t>());
    ofs.imbue(loc);
}

bool FileTextOutputStream::write(const String& text) {
    ofs << text.toUnicode();
    return bool(ofs);
}

FileBinaryInputStream::FileBinaryInputStream(const Path& path)
    : ifs(path.native(), std::ios::in | std::ios::binary) {
    if (!ifs) {
        throw Exception(format("Cannot open file {} for reading.", path.string()));
    }
}

FileTextInputStream::FileTextInputStream(const Path& path) {
    ifs.open(path.native());
    std::locale loc(std::locale(), new std::codecvt_utf8<wchar_t>());
    ifs.imbue(loc);
}

bool FileTextInputStream::readLine(String& text, const wchar_t delimiter) {
    std::wstring wstr;
    if (std::getline(ifs, wstr, delimiter)) {
        text = String::fromWstring(wstr);
        return true;
    } else {
        return false;
    }
}

bool FileTextInputStream::readAll(String& text) {
    std::wstringstream buffer;
    buffer << ifs.rdbuf();
    text = String::fromWstring(buffer.str());
    return bool(ifs);
}

StringTextInputStream::StringTextInputStream(const String& string)
    : ifs(string.toUnicode()) {}

bool StringTextInputStream::readLine(String& text, const wchar_t delimiter) {
    std::wstring wstr;
    if (std::getline(ifs, wstr, delimiter)) {
        text = String::fromWstring(wstr);
        return true;
    } else {
        return false;
    }
}

bool StringTextInputStream::readAll(String& text) {
    text = String::fromWstring(ifs.str());
    return bool(ifs);
}

bool StringTextOutputStream::write(const String& text) {
    ofs << text.toUnicode();
    return bool(ofs);
}

NAMESPACE_SPH_END
