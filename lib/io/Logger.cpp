#include "io/Logger.h"
#include "common/Assert.h"
#include <fstream>
#include <iostream>

NAMESPACE_SPH_BEGIN

void StdOutLogger::writeString(const std::string& s) {
    std::cout << s << std::flush;
}

FileLogger::FileLogger(const std::string& path, const Flags<Options> flags)
    : path(path)
    , flags(flags) {
    stream = std::make_unique<std::ofstream>();
    if (!flags.has(Options::OPEN_WHEN_WRITING)) {
        auto mode = flags.has(Options::APPEND) ? std::ostream::app : std::ostream::out;
        stream->open(path.c_str(), mode);
        if (!*stream) {
            throw IoError("Error opening FileLogger at " + path);
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
        stream->open(path.c_str(), std::ostream::app);
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
