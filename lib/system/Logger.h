#pragma once

/// Output routines of the run.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"
#include <fstream>
#include <iostream>
#include <set>
#include <string>

NAMESPACE_SPH_BEGIN

/// Interface providing generic (text, human readable) output of the program.
namespace Abstract {
    class Logger : public Polymorphic {
    public:
        virtual void write(const std::string& s) = 0;

        Logger& operator<<(const std::string& s) {
            write(s);
            return *this;
        }
    };
}

/// Standard output logger.
class StdOutput : public Abstract::Logger {
public:
    virtual void write(const std::string& s) override { std::cout << s << std::endl; }
};

/// File output logger
class FileOutput : public Abstract::Logger {
private:
    std::ofstream stream;

public:
    FileOutput(const std::string& path)
        : stream(path, std::ofstream::out) {}

    ~FileOutput() {
        stream.close();
    }

    virtual void write(const std::string& s) override { stream << s; }
};

/// Class holding multiple loggers and writing messages to all of them. The objects is the owner of loggers.
class MultiLogger : public Abstract::Logger {
private:
    std::set<std::unique_ptr<Abstract::Logger>> loggers;

public:
    int getLoggerCnt() const { return loggers.size(); }

    void add(Abstract::Logger* logger) { loggers.insert(std::unique_ptr<Abstract::Logger>(logger)); }

    virtual void write(const std::string& s) override {
        for (auto& l : loggers) {
            l->write(s);
        }
    }
};

NAMESPACE_SPH_END
