#include "io/Logger.h"
#include "common/Assert.h"
#include "io/FileSystem.h"
#include "objects/Exceptions.h"
#include "objects/containers/StaticArray.h"
#include "objects/utility/Streams.h"
#include <iostream>

#ifdef SPH_WIN
#include <Windows.h>
#endif

NAMESPACE_SPH_BEGIN

ScopedConsole::ScopedConsole(const Console console) {
    std::wcout << console;
}

ScopedConsole::~ScopedConsole() {
    std::wcout << Console::Foreground::DEFAULT << Console::Background::DEFAULT << Console::Series::NORMAL;
}

void StdOutLogger::writeString(const String& s) {
    std::wcout << s << std::flush;
}

#ifdef SPH_WIN

void ConsoleLogger::writeString(const String& s) {
    OutputDebugStringW(s.toUnicode());
}

#endif

void StringLogger::writeString(const String& s) {
    ss << s << std::flush;
}

void StringLogger::clean() {
    // clear internal storage
    ss.str(std::wstring());
    // reset flags
    ss.clear();
}

String StringLogger::toString() const {
    return String::fromWstring(ss.str());
}


FileLogger::FileLogger(const Path& path, const Flags<Options> flags)
    : path(path)
    , flags(flags) {
    auto mode = flags.has(Options::APPEND) ? FileTextOutputStream::OpenMode::APPEND
                                           : FileTextOutputStream::OpenMode::WRITE;
    FileSystem::createDirectory(path.parentPath());
    stream = makeAuto<FileTextOutputStream>(path, mode);
    if (!stream->good()) {
        throw IoError(L"Error opening FileLogger at " + path.string());
    }
}

FileLogger::~FileLogger() = default;

void FileLogger::writeString(const String& s) {
    if (flags.has(Options::ADD_TIMESTAMP)) {
        std::time_t t = std::time(nullptr);
        stream->write() << std::put_time(std::localtime(&t), L"%b %d, %H:%M:%S -- ");
    }
    stream->write() << s << std::flush;
}

struct VerboseLogThreadContext {
    AutoPtr<ILogger> logger = makeAuto<NullLogger>();
    int indent = 0;
};

static VerboseLogThreadContext context;

VerboseLogGuard::VerboseLogGuard(const String& functionName) {
    if (!context.logger) {
        // quick exit in case no logger is used
        return;
    }
    // remove unneeded parts of the pretty name
    String printedName = functionName;
    Size n = printedName.find('(');
    if (n != String::npos) {
        // no need for the list of parameters
        printedName = printedName.substr(0, n);
    }
    // remove unnecessary return types, common namespaces, ...
    printedName.replaceAll("Sph::", "");
    printedName.replaceFirst("virtual ", "");
    printedName.replaceFirst("void ", "");
    printedName.replaceFirst("int ", "");
    printedName.replaceFirst("auto ", "");

    const String prefix = String::fromChar(' ', 4 * context.indent) + toString(context.indent);
    context.logger->writeString(prefix + "-" + printedName + "\n");
    context.indent++;
}

VerboseLogGuard::~VerboseLogGuard() {
    if (!context.logger) {
        // quick exit in case no logger is used
        return;
    }

    --context.indent;
    const String prefix = String::fromChar(' ', 4 * context.indent);
    context.logger->writeString(
        prefix + "  took " + toString(int(timer.elapsed(TimerUnit::MILLISECOND))) + "ms\n");
    SPH_ASSERT(context.indent >= 0);
}

void setVerboseLogger(AutoPtr<ILogger>&& logger) {
    SPH_ASSERT(context.indent == 0);
    context.logger = std::move(logger);
    context.indent = 0;
}


NAMESPACE_SPH_END
