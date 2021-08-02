#pragma once

#include "io/Logger.h"
#include "objects/Exceptions.h"
#include "objects/containers/FlatMap.h"
#include "objects/wrappers/Function.h"
#include "objects/wrappers/SharedPtr.h"
#include "objects/wrappers/Variant.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

enum class ArgEnum {
    NONE, ///< No value after the argument
    BOOL,
    INT,
    FLOAT,
    STRING,
};


/// \brief Exception thrown if the arguments are invalid
class ArgError : public Exception {
public:
    explicit ArgError(const std::string& message)
        : Exception(message) {}
};

/// \brief Exception thrown when used passes -h or --help parameter.
///
/// The exception message contains the parameter description.
class HelpException : public Exception {
public:
    explicit HelpException(const std::string& message)
        : Exception(message) {}
};

/// \brief Descriptor of a command-line argument.
struct ArgDesc {
    /// Short name, prefixed by single dash (e.g. -h)
    std::string shortName;

    /// Long name, prefixed by double-dash (e.g. --help)
    std::string longName;

    /// Type of the parameter
    ArgEnum type;

    /// Parameter description, printed in help
    std::string desc;

    /// Generic callback executed when the parameter is parsed
    Function<void()> callback = nullptr;

    /// \brief Checks if the descriptor matches given argument.
    bool matches(const std::string& name) {
        return (name == "-" + shortName) || (name == "--" + longName);
    }
};

/// \brief Provides functions for parsing command-line arguments.
class ArgParser {
private:
    using ArgValue = Variant<int, Float, bool, std::string>;

    /// Input argument descriptors
    Array<ArgDesc> descs;

    /// Parsed argument values
    FlatMap<std::string, ArgValue> params;

public:
    /// \brief Creates a parser, given a set of parameter descriptors.
    ///
    /// The "help" parameter (-h or --help) is automatically added into the set. It can be used to print the
    /// program help, using data provided in the descriptors.
    explicit ArgParser(ArrayView<const ArgDesc> args);

    /// \brief Parses the input arguments and stored the parsed values.
    ///
    /// Parsed values can be later queried using \ref getArg or \ref forEach functions. Previous values are
    /// removed.
    /// \throw ArgError if the input is not valid.
    void parse(const int argc, char** argv);

    /// \brief Prints the help information into given logger.
    ///
    /// This is automatically called when \ref parse is called with argument -h or --help.
    void printHelp(ILogger& logger);

    /// \brief Returns the value of an argument, given its short name (without the dash).
    ///
    /// \throw ArgError if the argument was not been parsed or it has different type.
    template <typename TValue>
    TValue getArg(const std::string& name) const {
        throwIfUnknownArg(name);
        Optional<const ArgValue&> value = params.tryGet(name);
        if (value && value->has<TValue>()) {
            return value->get<TValue>();
        } else {
            const std::string message = value ? "Invalid type of argument -" : "Missing argument -" + name;
            throw ArgError(message);
        }
    }

    /// \brief Returns the value of an argument or NOTHING if the argument was not parsed.
    ///
    /// \throw ArgError if the name does not match any argument descriptor or it has different type.
    template <typename TValue>
    Optional<TValue> tryGetArg(const std::string& name) const {
        throwIfUnknownArg(name);
        Optional<const ArgValue&> value = params.tryGet(name);
        if (value) {
            if (!value->has<TValue>()) {
                throw ArgError("Invalid type");
            }
            return value->get<TValue>();
        } else {
            return NOTHING;
        }
    }

    /// \brief Stores the value of given argument into an instance of \ref Settings.
    ///
    /// If the value was not parsed, no value is stored and function returns false. Otherwise, true is
    /// returned.
    /// \param settings \ref Settings object where the parsed value is stored (if present).
    /// \param name Short name identifying the argument.
    /// \param idx Index of the target entry in \ref Settings.
    /// \param conv Generic value convertor, applied on value saved to \ref Settings. Applied only on float
    ///             arguments.
    /// \throw ArgError if the name does not match any argument descriptor.
    template <typename TEnum, typename TConvertor>
    bool tryStore(Settings<TEnum>& settings, const std::string& name, const TEnum idx, TConvertor&& conv) {
        throwIfUnknownArg(name);
        auto value = params.tryGet(name);
        if (value) {
            // special handling of floats - convert units
            if (value->has<Float>()) {
                settings.set(idx, conv(value->get<Float>()));
            } else {
                forValue(value.value(), [&settings, idx](auto& value) { settings.set(idx, value); });
            }
            return true;
        }
        return false;
    }

    /// \brief Stores the value of given argument into an instance of \ref Settings.
    ///
    /// Overload without value convertor.
    template <typename TEnum>
    bool tryStore(Settings<TEnum>& settings, const std::string& name, const TEnum idx) {
        return tryStore(settings, name, idx, [](Float value) { return value; });
    }

    /// \brief Enumerates all parsed arguments and executes a generic functor with parsed values.
    ///
    /// Functor must be either generic lambda, or it must overload operator() for each parsed type, as listed
    /// in enum \ref ArgEnum. The operator must have signature void(std::string, Type). The arguments are
    /// identified using its short name, without the dash.
    template <typename TFunctor>
    void forEach(const TFunctor& functor) {
        for (auto& e : params) {
            forValue(e.value(), [&e, &functor](auto& value) { functor(e.key(), value); });
        }
    }

    /// \brief Returns the number of parsed arguments.
    Size size() const {
        return params.size();
    }

    /// \brief Returns true if no arguments have been parsed.
    bool empty() const {
        return params.empty();
    }

private:
    void parseValuelessArg(const ArgDesc& desc);

    void parseValueArg(const ArgDesc& desc, const std::string& value);

    template <typename TValue>
    void insertArg(const std::string& name, const std::string& textValue);

    void throwIfUnknownArg(const std::string& name) const;
};


NAMESPACE_SPH_END
