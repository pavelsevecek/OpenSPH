#pragma once

/// \file Logger.h
/// \brief Logging routines of the run.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/Globals.h"
#include "io/Path.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/AutoPtr.h"
#include "objects/wrappers/Flags.h"
#include "system/Timer.h"
#include <iomanip>

NAMESPACE_SPH_BEGIN

class FileTextOutputStream;

/// \brief Interface providing generic (text, human readable) output of the program.
///
/// It's meant for logging current time, some statistics of the simulation, encountered warnings and errors,
/// etc. For output of particle quantities, use \ref IOutput.
class ILogger : public Polymorphic, public Noncopyable {
private:
    Size precision = PRECISION;
    bool scientific = true;

public:
    /// \brief Logs a string message.
    ///
    /// \todo different types (log, warning, error, ...) and levels of verbosity
    virtual void writeString(const String& s) = 0;

    /// \brief Creates and logs a message by concatenating arguments.
    ///
    /// Adds a new line to the output.
    template <typename... TArgs>
    void write(TArgs&&... args) {
        std::wstringstream ss;
        writeImpl(ss, std::forward<TArgs>(args)...);
        ss << std::endl;
        this->writeString(String::fromWstring(ss.str()));
    }

    /// \brief Changes the precision of printed numbers.
    ///
    /// The default value is given by global PRECISION constant.
    void setPrecision(const Size newPrecision) {
        precision = newPrecision;
    }

    /// \brief Sets/unsets scientific notation
    void setScientific(const bool newScientific) {
        scientific = newScientific;
    }

private:
    template <typename T0, typename... TArgs>
    void writeImpl(std::wstringstream& ss, T0&& first, TArgs&&... rest) {
        ss << std::setprecision(precision);
        if (scientific) {
            ss << std::scientific;
        }
        ss << first;
        writeImpl(ss, std::forward<TArgs>(rest)...);
    }
    void writeImpl(std::wstringstream& UNUSED(ss)) {}
};


struct Console {
    enum class Foreground {
        BLACK = 30,
        RED = 31,
        GREEN = 32,
        YELLOW = 33,
        BLUE = 34,
        MAGENTA = 35,
        CYAN = 36,
        LIGHT_GRAY = 37,
        DARK_GRAY = 90,
        LIGHT_RED = 91,
        LIGHT_GREEN = 92,
        LIGHT_YELLOW = 93,
        LIGHT_BLUE = 94,
        LIGHT_MAGENTA = 95,
        LIGHT_CYAN = 96,
        WHITE = 97,
        DEFAULT = 39,
        UNCHANGED = 0,
    } fg = Foreground::UNCHANGED;

    enum class Background {
        RED = 41,
        GREEN = 42,
        BLUE = 44,
        DEFAULT = 49,
        UNCHANGED = 0
    } bg = Background::UNCHANGED;

    enum class Series {
        NORMAL = 0,
        BOLD = 1,
    } series = Series::NORMAL;

    Console() = default;

    Console(const Foreground fg)
        : fg(fg) {}

    Console(const Background bg)
        : bg(bg) {}

    Console(const Series series)
        : series(series) {}

    friend std::wostream& operator<<(std::wostream& stream, const Console& mod) {
        if (mod.bg != Background::UNCHANGED) {
            stream << "\033[" << int(mod.bg) << "m";
        }
        if (mod.fg != Foreground::UNCHANGED) {
            stream << "\033[" << int(mod.fg) << "m";
        }
#ifndef SPH_WIN
        stream << "\e[" << int(mod.series) << "m";
#endif
        return stream;
    }
};

struct ScopedConsole {
    ScopedConsole(const Console console);
    ~ScopedConsole();
};


/// \brief Standard output logger.
///
/// This is just a wrapper of std::cout with ILogger interface, it does not tructed on spot with no
/// cost. All StdOutLoggers print to the same output.
class StdOutLogger : public ILogger {
public:
    virtual void writeString(const String& s) override;
};

#ifdef SPH_WIN
/// \brief Writes into the visual studio console
class ConsoleLogger : public ILogger {
public:
    virtual void writeString(const String& s) override;
};
#endif

/// \brief Logger writing messages to string stream
class StringLogger : public ILogger {
private:
    std::wstringstream ss;

public:
    virtual void writeString(const String& s) override;

    /// Removes all written messages from the string.
    void clean();

    /// Returns all written messages as a string. Messages are not erased from the logger by this
    String toString() const;
};

/// File output logger
class FileLogger : public ILogger {
public:
    enum class Options {
        /// If the file already exists, the new messages are appended to existing content instead of erasing
        /// the file.
        APPEND = 1 << 1,

        /// Adds a time of writing before each message.
        ADD_TIMESTAMP = 1 << 2,
    };

private:
    AutoPtr<FileTextOutputStream> stream;
    Path path;
    Flags<Options> flags;

public:
    FileLogger(const Path& path, const Flags<Options> flags = EMPTY_FLAGS);

    ~FileLogger();

    virtual void writeString(const String& s) override;
};

/// \brief Class holding multiple loggers and writing messages to all of them.
///
/// The objects is the owner of loggers.
class MultiLogger : public ILogger {
private:
    Array<AutoPtr<ILogger>> loggers;

public:
    Size getLoggerCnt() const {
        return loggers.size();
    }

    void add(AutoPtr<ILogger>&& logger) {
        loggers.push(std::move(logger));
    }

    virtual void writeString(const String& s) override {
        for (auto& l : loggers) {
            l->writeString(s);
        }
    }
};

/// \brief Helper logger that does not write anything.
class NullLogger : public ILogger {
public:
    virtual void writeString(const String& UNUSED(s)) override {}
};

/// \brief RAII guard writing called functions and their durations to a special verbose logger.
class VerboseLogGuard : public Noncopyable {
private:
    Timer timer;

public:
    /// \brief Creates a guard, should be at the very beginning of a function/scope.
    VerboseLogGuard(const String& functionName);

    ~VerboseLogGuard();
};

/// \brief Creates a global verbose logger.
///
/// Provided logger is stored and subsequently used by all \ref VerboseLogGuard objects. There can be only one
/// logger at the same time. Verbose logging can be disabled by passing nullptr into the function.
void setVerboseLogger(AutoPtr<ILogger>&& logger);

/// \brief Helper macro, creating \brief VerboseLogGuard with name of the current function.
#define VERBOSE_LOG VerboseLogGuard __verboseLogGuard(SPH_PRETTY_FUNCTION);

NAMESPACE_SPH_END
