#pragma once

#include "common/Globals.h"
#include <exception>
#include <string>

NAMESPACE_SPH_BEGIN

/// Thrown when components of the run are mutually incompatible.
class InvalidSetup : public std::exception {
private:
    std::string message;

public:
    InvalidSetup(const std::string& message)
        : message(message) {}

    virtual const char* what() const noexcept {
        return message.c_str();
    }
};

NAMESPACE_SPH_END
