#pragma once

/// Logging routines of the run.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"
#include <memory>
#include <set>
#include <fstream>
#include <sstream>
#include <string>

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
            ss << first;
            writeImpl(ss, std::forward<TArgs>(rest)...);
        }
        void writeImpl(std::stringstream& UNUSED(ss)) {}
    };
}

/// Standard output logger.
class StdOutLogger : public Abstract::Logger {
public:
    virtual void writeString(const std::string& s) override;
};

/// File output logger
class FileLogger : public Abstract::Logger {
private:
    std::ofstream stream;

public:
    FileLogger(const std::string& path)
        : stream(path, std::ofstream::out) {}

    ~FileLogger() { stream.close(); }

    virtual void writeString(const std::string& s) override { stream << s; }
};

/// Class holding multiple loggers and writing messages to all of them. The objects is the owner of loggers.
class MultiLogger : public Abstract::Logger {
private:
    std::set<std::unique_ptr<Abstract::Logger>> loggers;

public:
    int getLoggerCnt() const { return loggers.size(); }

    void add(std::unique_ptr<Abstract::Logger>&& logger) { loggers.insert(std::move(logger)); }

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
