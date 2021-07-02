#pragma once

#include "common/Globals.h"
#include <exception>
#include <string>

NAMESPACE_SPH_BEGIN

/// \brief Generic exception
class Exception : public std::exception {
private:
    std::string message;

public:
    explicit Exception(const std::string& message)
        : message(message) {}

    virtual const char* what() const noexcept {
        return message.c_str();
    }
};

/// \brief Thrown when components of the run are mutually incompatible.
class InvalidSetup : public Exception {
public:
    explicit InvalidSetup(const std::string& message)
        : Exception(message) {}
};

/// \brief Exception thrown when file cannot be read, it has invalid format, etc.
class IoError : public Exception {
public:
    explicit IoError(const std::string& message)
        : Exception(message) {}
};

NAMESPACE_SPH_END
