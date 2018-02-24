#pragma once

/// \file Logger.h
/// \brief Logging routines of the run.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "common/Globals.h"
#include "io/Path.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/AutoPtr.h"
#include "objects/wrappers/Flags.h"
#include <iomanip>

NAMESPACE_SPH_BEGIN

/// \brief Interface providing generic (text, human readable) output of the program.
///
/// It's meant for logging current time, some statistics of the simulation, encountered warnings and errors,
/// etc. For output of particle quantities, use IOutput.
class ILogger : public Polymorphic, public Noncopyable {
private:
    Size precision = PRECISION;
    bool scientific = true;

public:
    /// Logs a string message.
    /// \todo different types (log, warning, error, ...) and levels of verbosity
    virtual void writeString(const std::string& s) = 0;

    /// Creates and logs message by concating arguments. Adds a new line to the output.
    template <typename... TArgs>
    void write(TArgs&&... args) {
        std::stringstream ss;
        writeImpl(ss, std::forward<TArgs>(args)...);
        ss << std::endl;
        this->writeString(ss.str());
    }

    /// Changes the precision of printed numbers. The default value is given by global PRECISION constant.
    void setPrecision(const Size newPrecision) {
        precision = newPrecision;
    }

    /// Sets/unsets scientific notation
    void setScientific(const bool newScientific) {
        scientific = newScientific;
    }

private:
    template <typename T0, typename... TArgs>
    void writeImpl(std::stringstream& ss, T0&& first, TArgs&&... rest) {
        ss << std::setprecision(precision);
        if (scientific) {
            ss << std::scientific;
        }
        ss << first;
        writeImpl(ss, std::forward<TArgs>(rest)...);
    }
    void writeImpl(std::stringstream& UNUSED(ss)) {}
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

    friend std::ostream& operator<<(std::ostream& stream, const Console& mod) {
        if (mod.bg != Background::UNCHANGED) {
            stream << "\033[" << int(mod.bg) << "m";
        }
        if (mod.fg != Foreground::UNCHANGED) {
            stream << "\033[" << int(mod.fg) << "m";
        }
        stream << "\e[" << int(mod.series) << "m";
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
    virtual void writeString(const std::string& s) override;
};


/// \brief Logger writing messages to string stream
class StringLogger : public ILogger {
private:
    std::stringstream ss;

public:
    virtual void writeString(const std::string& s) override;

    /// Removes all written messages from the string.
    void clean();

    /// Returns all written messages as a string. Messages are not erased from the logger by this
    std::string toString() const;
};

/// Exception thrown by FileLogger
class IoError : public std::exception {
private:
    std::string message;

public:
    explicit IoError(const std::string& message)
        : message(message) {}

    virtual const char* what() const noexcept override {
        return message.c_str();
    }
};

/// File output logger
class FileLogger : public ILogger {
public:
    enum class Options {
        /// Opens the associated file when the logger is constructed and closes it in destructor. This is
        /// the default behavior of the logger, nevertheless this option can be explicitly specified to
        /// increase readability of the code.
        KEEP_OPENED = 0,

        /// Open the file only for writing and close it immediately afterwards. If the file cannot be opened,
        /// the message is not written into the log without throwing any exception. This option implies
        /// appending to existing content.
        OPEN_WHEN_WRITING = 1 << 0,

        /// If the file already exists, the new messages are appended to existing content instead of erasing
        /// the file.
        APPEND = 1 << 1,

        /// Adds a time of writing before each message.
        ADD_TIMESTAMP = 1 << 2,
    };

private:
    AutoPtr<std::ofstream> stream;
    Path path;
    Flags<Options> flags;

public:
    FileLogger(const Path& path, const Flags<Options> flags = EMPTY_FLAGS);

    ~FileLogger();

    virtual void writeString(const std::string& s) override;
};

/// Class holding multiple loggers and writing messages to all of them. The objects is the owner of loggers.
class MultiLogger : public ILogger {
private:
    Array<AutoPtr<ILogger>> loggers;

public:
    int getLoggerCnt() const {
        return loggers.size();
    }

    void add(AutoPtr<ILogger>&& logger) {
        loggers.push(std::move(logger));
    }

    virtual void writeString(const std::string& s) override {
        for (auto& l : loggers) {
            l->writeString(s);
        }
    }
};

class DummyLogger : public ILogger {
    virtual void writeString(const std::string& UNUSED(s)) override {}
};

NAMESPACE_SPH_END
