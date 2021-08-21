#pragma once

#include "common/Globals.h"
#include "objects/containers/String.h"
#include <exception>

NAMESPACE_SPH_BEGIN

/// \brief Generic exception
class Exception : public std::exception {
private:
    CharString message;

public:
    template <typename... TArgs>
    explicit Exception(const String& message, const TArgs&... args)
        : message(format(message, args...).toUtf8()) {}

    virtual const char* what() const noexcept {
        return message;
    }
};

/// \brief Thrown when components of the run are mutually incompatible, parameters have invalid values, etc.
class InvalidSetup : public Exception {
public:
    using Exception::Exception;
};

/// \brief Throws when a data-dependend error is encountered (all particles got removed, etc.)
class DataException : public Exception {
public:
    using Exception::Exception;
};

/// \brief Exception thrown when file cannot be read, it has invalid format, etc.
class IoError : public Exception {
public:
    using Exception::Exception;
};

NAMESPACE_SPH_END
