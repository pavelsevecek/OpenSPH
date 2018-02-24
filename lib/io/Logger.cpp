#include "io/Logger.h"
#include "common/Assert.h"
#include "io/FileSystem.h"
#include <fstream>
#include <iostream>

NAMESPACE_SPH_BEGIN

ScopedConsole::ScopedConsole(const Console console) {
    std::cout << console;
}

ScopedConsole::~ScopedConsole() {
    std::cout << Console::Foreground::DEFAULT << Console::Background::DEFAULT << Console::Series::NORMAL;
}

void StdOutLogger::writeString(const std::string& s) {
    std::cout << s << std::flush;
}

void StringLogger::writeString(const std::string& s) {
    ss << s << std::flush;
}

void StringLogger::clean() {
    // clear internal storage
    ss.str(std::string());
    // reset flags
    ss.clear();
}

std::string StringLogger::toString() const {
    return ss.str();
}


FileLogger::FileLogger(const Path& path, const Flags<Options> flags)
    : path(path)
    , flags(flags) {
    stream = makeAuto<std::ofstream>();
    if (!flags.has(Options::OPEN_WHEN_WRITING)) {
        auto mode = flags.has(Options::APPEND) ? std::ostream::app : std::ostream::out;
        FileSystem::createDirectory(path.parentPath());
        stream->open(path.native(), mode);
        if (!*stream) {
            throw IoError("Error opening FileLogger at " + path.native());
        }
    }
}

FileLogger::~FileLogger() = default;

void FileLogger::writeString(const std::string& s) {
    auto write = [this](const std::string& s) {
        if (flags.has(Options::ADD_TIMESTAMP)) {
            std::time_t t = std::time(nullptr);
            *stream << std::put_time(std::localtime(&t), "%b %d, %H:%M:%S -- ");
        }
        *stream << s;
    };

    if (flags.has(Options::OPEN_WHEN_WRITING)) {
        stream->open(path.native(), std::ostream::app);
        if (*stream) {
            write(s);
            stream->close();
        }
    } else {
        ASSERT(stream->is_open());
        write(s);
    }
}

NAMESPACE_SPH_END
