#include "io/Logger.h"
#include "common/Assert.h"
#include "io/FileSystem.h"
#include "objects/Exceptions.h"
#include "objects/containers/StaticArray.h"
#include "objects/utility/StringUtils.h"
#include <fstream>
#include <iostream>

#ifdef SPH_WIN
#include <Windows.h>
#endif

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

#ifdef SPH_WIN

void ConsoleLogger::writeString(const std::string& s) {
    OutputDebugStringA(s.c_str());
}

#endif

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
        *stream << s << std::flush;
    };

    if (flags.has(Options::OPEN_WHEN_WRITING)) {
        stream->open(path.native(), std::ostream::app);
        if (*stream) {
            write(s);
            stream->close();
        }
    } else {
        SPH_ASSERT(stream->is_open());
        write(s);
    }
}

struct VerboseLogThreadContext {
    AutoPtr<ILogger> logger = makeAuto<NullLogger>();
    int indent = 0;
};

static VerboseLogThreadContext context;

VerboseLogGuard::VerboseLogGuard(const std::string& functionName) {
    if (!context.logger) {
        // quick exit in case no logger is used
        return;
    }
    // remove unneeded parts of the pretty name
    std::string printedName = functionName;
    std::size_t n = printedName.find('(');
    if (n != std::string::npos) {
        // no need for the list of parameters
        printedName = printedName.substr(0, n);
    }
    // remove unnecessary return types, common namespaces, ...
    printedName = replaceAll(printedName, "Sph::", "");
    printedName = replaceFirst(printedName, "virtual ", "");
    printedName = replaceFirst(printedName, "void ", "");
    printedName = replaceFirst(printedName, "int ", "");
    printedName = replaceFirst(printedName, "auto ", "");

    const std::string prefix = std::string(4 * context.indent, ' ') + std::to_string(context.indent);
    context.logger->writeString(prefix + "-" + printedName + "\n");
    context.indent++;
}

VerboseLogGuard::~VerboseLogGuard() {
    if (!context.logger) {
        // quick exit in case no logger is used
        return;
    }

    --context.indent;
    const std::string prefix = std::string(4 * context.indent, ' ');
    context.logger->writeString(
        prefix + "  took " + std::to_string(int(timer.elapsed(TimerUnit::MILLISECOND))) + "ms\n");
    SPH_ASSERT(context.indent >= 0);
}

void setVerboseLogger(AutoPtr<ILogger>&& logger) {
    SPH_ASSERT(context.indent == 0);
    context.logger = std::move(logger);
    context.indent = 0;
}


NAMESPACE_SPH_END
