#pragma once

/// \file Logger.h
/// \brief Logging routines of the run.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/Globals.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/AutoPtr.h"
#include "objects/wrappers/Flags.h"
#include <iomanip>

NAMESPACE_SPH_BEGIN

/// Interface providing generic (text, human readable) output of the program. It's meant for logging current
/// time, some statistics of the simulation, encountered warnings and errors, etc. For output of particle
/// quantities, use Abstract::Output.
namespace Abstract {
    class Logger : public Polymorphic, public Noncopyable {
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

    private:
        template <typename T0, typename... TArgs>
        void writeImpl(std::stringstream& ss, T0&& first, TArgs&&... rest) {
            ss << std::setprecision(PRECISION) << std::scientific << first;
            writeImpl(ss, std::forward<TArgs>(rest)...);
        }
        void writeImpl(std::stringstream& UNUSED(ss)) {}
    };
}

/// Standard output logger. This is just a wrapper of std::cout with Abstract::Logger interface, it does not
/// hold any state and can be constructed on spot with no cost. All StdOutLoggers print to the same output.
class StdOutLogger : public Abstract::Logger {
public:
    virtual void writeString(const std::string& s) override;
};

class IoError : public std::exception {
private:
    std::string message;

public:
    IoError(const std::string& message)
        : message(message) {}

    virtual const char* what() const noexcept override {
        return message.c_str();
    }
};

/// File output logger
class FileLogger : public Abstract::Logger {
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
    std::string path;
    Flags<Options> flags;

public:
    FileLogger(const std::string& path, const Flags<Options> flags = EMPTY_FLAGS);

    ~FileLogger();

    virtual void writeString(const std::string& s) override;
};

/// Class holding multiple loggers and writing messages to all of them. The objects is the owner of loggers.
class MultiLogger : public Abstract::Logger {
private:
    Array<AutoPtr<Abstract::Logger>> loggers;

public:
    int getLoggerCnt() const {
        return loggers.size();
    }

    void add(AutoPtr<Abstract::Logger>&& logger) {
        loggers.push(std::move(logger));
    }

    virtual void writeString(const std::string& s) override {
        for (auto& l : loggers) {
            l->writeString(s);
        }
    }
};

class DummyLogger : public Abstract::Logger {
    virtual void writeString(const std::string& UNUSED(s)) override {}
};

NAMESPACE_SPH_END
