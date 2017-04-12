#include "io/Logger.h"
#include "common/Assert.h"
#include <iostream>

NAMESPACE_SPH_BEGIN

void StdOutLogger::writeString(const std::string& s) {
    std::cout << s << std::flush;
}

FileLogger::FileLogger(const std::string& path, const Flags<Options> flags)
    : path(path)
    , flags(flags) {
    if (!flags.has(Options::OPEN_WHEN_WRITING)) {
        stream.open(path, flags.has(Options::APPEND) ? std::ofstream::app : std::ofstream::out);
    }
}

FileLogger::~FileLogger() = default;

void FileLogger::writeString(const std::string& s) {
    auto write = [this](const std::string& s) {
        if (flags.has(Options::ADD_TIMESTAMP)) {
            std::time_t t = std::time(nullptr);
            stream << std::put_time(std::localtime(&t), "%b %d, %H:%M:%S -- ");
        }
        stream << s;
    };

    if (flags.has(Options::OPEN_WHEN_WRITING)) {
        try {
            stream.open(path, std::ofstream::app);
            write(s);
            stream.close();
        } catch (...) {
            // silence all exceptions
        }
    } else {
        ASSERT(stream.is_open());
        write(s);
    }
}

NAMESPACE_SPH_END
